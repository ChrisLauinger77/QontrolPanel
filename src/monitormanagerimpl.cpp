#include "monitormanagerimpl.h"
#include "logmanager.h"
#include "nightlightdata.h"
#include <algorithm>
#include <qlogging.h>
#include <vector>

MonitorManagerImpl::MonitorManagerImpl()
    : messageWindow(nullptr)
    , pWMIService(nullptr)
    , m_nightLightRegKey(nullptr)
{
    LOG_INFO("MonitorManager", "Initializing MonitorManager");

    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_comInitialized = SUCCEEDED(hr);
    if (!m_comInitialized)
        LOG_WARN("MonitorManager", QString("COM initialization failed: %1").arg(QString::number(hr, 16)));

    // Initialize components
    initNightLightRegistry();
    setupChangeDetection();

    LOG_INFO("MonitorManager", "MonitorManager initialization complete");
}

MonitorManagerImpl::~MonitorManagerImpl() {
    LOG_INFO("MonitorManager", "Cleaning up MonitorManager");
    cleanup();
    cleanupNightLightRegistry();
    cleanupWMI();
    if (messageWindow)
        DestroyWindow(messageWindow);
    if (m_comInitialized)
    CoUninitialize();
}

bool MonitorManagerImpl::ensureWMIConnection() {
    std::lock_guard<std::mutex> lock(m_wmiMutex);

    auto now = std::chrono::steady_clock::now();

    // If WMI was recently initialized and passes health check, keep it
    if (pWMIService &&
        m_wmiInitialized.load() &&
        (now - m_lastWMIInit) < WMI_CACHE_DURATION &&
        quickWMIHealthCheck()) {
        return true;
    }

    // Clean up existing connection
    if (pWMIService) {
        pWMIService->Release();
        pWMIService = nullptr;
    }

    // Initialize fresh connection
    initializeWMI();
    m_lastWMIInit = now;
    m_wmiInitialized.store(pWMIService != nullptr);

    return pWMIService != nullptr;
}

bool MonitorManagerImpl::quickWMIHealthCheck() {
    if (!pWMIService) return false;

    // Quick health check without debug spam
    IEnumWbemClassObject* pTest = nullptr;
    HRESULT testResult = pWMIService->ExecQuery(bstr_t("WQL"), bstr_t("SELECT * FROM WmiMonitorBrightness"),
                                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pTest);

    bool healthy = SUCCEEDED(testResult);
    if (pTest) pTest->Release();

    if (!healthy) {
        LOG_WARN("MonitorManager",
                 QString("WMI health check failed: %1").arg(QString::number(testResult, 16)));
    }

    return healthy;
}

void MonitorManagerImpl::initializeWMI() {
    LOG_INFO("MonitorManager", "Initializing WMI connection");

    // Initialize security
    HRESULT hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_NONE,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
        );

    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        LOG_WARN("MonitorManager",
                 QString("CoInitializeSecurity failed: %1").arg(QString::number(hres, 16)));
    }

    // Create WMI locator
    IWbemLocator* pLoc = nullptr;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        LOG_CRITICAL("MonitorManager",
                     QString("Failed to create WMI locator: %1").arg(QString::number(hres, 16)));
        return;
    }

    // Connect to WMI namespace
    hres =
        pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, WBEM_FLAG_CONNECT_USE_MAX_WAIT, 0, 0, &pWMIService);
    if (FAILED(hres)) {
        LOG_CRITICAL("MonitorManager",
                     QString("WMI ConnectServer failed: %1").arg(QString::number(hres, 16)));
        pLoc->Release();
        return;
    }

    // Set proxy blanket
    hres = CoSetProxyBlanket(pWMIService, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                             RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) {
        LOG_CRITICAL("MonitorManager",
                     QString("CoSetProxyBlanket failed: %1").arg(QString::number(hres, 16)));
        pWMIService->Release();
        pWMIService = nullptr;
    } else {
        LOG_INFO("MonitorManager", "WMI connection established successfully");
    }

    pLoc->Release();
}

void MonitorManagerImpl::cleanupWMI() {
    std::lock_guard<std::mutex> lock(m_wmiMutex);
    if (pWMIService) {
        pWMIService->Release();
        pWMIService = nullptr;
        LOG_INFO("MonitorManager", "WMI connection cleaned up");
    }
    m_wmiInitialized.store(false);
}

void MonitorManagerImpl::enumerateMonitors() {
    LOG_INFO("MonitorManager", "Enumerating monitors");
    cleanup();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)this);
    detectLaptopDisplays();
    LOG_INFO("MonitorManager",
             QString("Monitor enumeration complete, found %1 monitors").arg(monitors.size()));
}

std::wstring MonitorManagerImpl::getMonitorName(int index) const {
    if (index < 0 || index >= monitors.size()) return L"";

    if (monitors[index].isLaptopDisplay) {
        return L"Laptop Internal Display";
    }
    return std::wstring(monitors[index].physicalMonitor.szPhysicalMonitorDescription);
}

bool MonitorManagerImpl::setBrightnessInternal(int monitorIndex, int brightness) {
    if (monitorIndex < 0 || monitorIndex >= monitors.size()) return false;
    if (brightness < 0 || brightness > 100) return false;

    LOG_INFO("MonitorManager",
             QString("Setting brightness to %1% for monitor %2").arg(brightness).arg(monitorIndex));

    if (monitors[monitorIndex].isLaptopDisplay) {
        bool result = setLaptopBrightness(brightness);
        if (result) {
            LOG_INFO("MonitorManager",
                     "Laptop brightness set successfully");
        } else {
            LOG_WARN("MonitorManager",
                     "Failed to set laptop brightness");
        }
        return result;
    } else {
        if (!testDDCCI(monitorIndex))
            return false;
        const DWORD nativeBrightness = static_cast<DWORD>(
            (static_cast<uint64_t>(brightness) * monitors[monitorIndex].maximumBrightness + 50) / 100);
        bool result = SetVCPFeature(monitors[monitorIndex].physicalMonitor.hPhysicalMonitor, 0x10, nativeBrightness);
        if (result) {
            monitors[monitorIndex].cachedBrightness = brightness;
            LOG_INFO("MonitorManager",
                     QString("External monitor %1 brightness set successfully").arg(monitorIndex));
        } else {
            LOG_WARN("MonitorManager",
                     QString("Failed to set brightness for external monitor %1").arg(monitorIndex));
        }
        return result;
    }
}

bool MonitorManagerImpl::setBrightnessAll(int brightness) {
    if (brightness < 0 || brightness > 100) return false;

    LOG_INFO("MonitorManager",
             QString("Setting brightness to %1% for all monitors").arg(brightness));

    bool allSuccess = true;
    for (int i = 0; i < getMonitorCount(); i++) {
        if (monitors[i].isLaptopDisplay || testDDCCI(i)) {
            bool success = setBrightnessInternal(i, brightness);
            if (!success) allSuccess = false;
        }
    }

    if (allSuccess) {
        LOG_INFO("MonitorManager",
                 "Successfully set brightness for all monitors");
    } else {
        LOG_WARN("MonitorManager",
                 "Failed to set brightness for some monitors");
    }

    return allSuccess;
}

int MonitorManagerImpl::getBrightnessInternal(int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= monitors.size()) return -1;

    if (monitors[monitorIndex].isLaptopDisplay) {
        return getLaptopBrightness();
    }

    DWORD current, max;
    if (GetVCPFeatureAndVCPFeatureReply(monitors[monitorIndex].physicalMonitor.hPhysicalMonitor, 0x10, NULL, &current, &max)) {
        monitors[monitorIndex].ddcciWorking = max > 0 && current <= max;
        if (!monitors[monitorIndex].ddcciWorking)
            return -1;
        monitors[monitorIndex].maximumBrightness = max;
        const int percent = static_cast<int>((static_cast<uint64_t>(current) * 100 + max / 2) / max);
        monitors[monitorIndex].cachedBrightness = percent;
        return percent;
    }

    monitors[monitorIndex].ddcciWorking = false;
    return -1;
}

bool MonitorManagerImpl::testDDCCI(int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= monitors.size()) return false;

    if (monitors[monitorIndex].isLaptopDisplay) {
        return true;
    }

    if (monitors[monitorIndex].ddcciTested) {
        return monitors[monitorIndex].ddcciWorking;
    }

    LOG_INFO("MonitorManager",
             QString("Testing DDC/CI support for monitor %1").arg(monitorIndex));

    monitors[monitorIndex].ddcciTested = true;
    bool result = getBrightnessInternal(monitorIndex) != -1;

    if (result) {
        LOG_INFO("MonitorManager",
                 QString("Monitor %1 supports DDC/CI").arg(monitorIndex));
    } else {
        LOG_WARN("MonitorManager",
                 QString("Monitor %1 does not support DDC/CI").arg(monitorIndex));
    }

    return result;
}

int MonitorManagerImpl::getCachedBrightness(int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= monitors.size()) return -1;
    return monitors[monitorIndex].cachedBrightness;
}

void MonitorManagerImpl::setChangeCallback(std::function<void()> callback) {
    changeCallback = callback;
}

void MonitorManagerImpl::detectLaptopDisplays() {
    LOG_INFO("MonitorManager", "Detecting laptop displays");

    bool wmiHasBrightnessSupport = false;

    if (ensureWMIConnection()) {
        // Check for WMI brightness methods
        IEnumWbemClassObject* pEnumerator = NULL;
        HRESULT hres = pWMIService->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT * FROM WmiMonitorBrightnessMethods"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL, &pEnumerator);

        if (SUCCEEDED(hres)) {
            IWbemClassObject* pclsObj = NULL;
            ULONG uReturn = 0;
            HRESULT hr = pEnumerator->Next(2000, 1, &pclsObj, &uReturn);
            if (uReturn > 0) {
                wmiHasBrightnessSupport = true;
                LOG_INFO("MonitorManager",
                         "WMI brightness support detected");
                if (pclsObj) pclsObj->Release();
            }
            pEnumerator->Release();
        } else {
            LOG_WARN("MonitorManager",
                     "Failed to query WMI brightness methods");
        }
    }

    if (wmiHasBrightnessSupport) {
        // Check if we already have a laptop display
        bool hasLaptopDisplay = false;
        for (const auto& monitor : monitors) {
            if (monitor.isLaptopDisplay) {
                hasLaptopDisplay = true;
                break;
            }
        }

        // If no laptop display exists, create one
        if (!hasLaptopDisplay) {
            MonitorInfo laptopMonitor = {};
            laptopMonitor.physicalMonitor.hPhysicalMonitor = NULL;
            wcscpy_s(laptopMonitor.physicalMonitor.szPhysicalMonitorDescription,
                     PHYSICAL_MONITOR_DESCRIPTION_SIZE, L"Laptop Internal Display");
            laptopMonitor.deviceName = L"LAPTOP";
            laptopMonitor.deviceId = L"wmi:internal";
            laptopMonitor.ddcciTested = true;
            laptopMonitor.ddcciWorking = false;
            laptopMonitor.isLaptopDisplay = true;
            laptopMonitor.cachedBrightness = getLaptopBrightness();

            monitors.insert(monitors.begin(), laptopMonitor);
            LOG_INFO("MonitorManager",
                     "Added laptop internal display to monitor list");
        } else {
            // Update existing laptop display brightness
            for (auto& monitor : monitors) {
                if (monitor.isLaptopDisplay) {
                    monitor.cachedBrightness = getLaptopBrightness();
                    break;
                }
            }
            LOG_INFO("MonitorManager",
                     "Updated existing laptop display brightness");
        }
    } else {
        LOG_INFO("MonitorManager",
                 "No laptop display brightness support detected");
    }
}

bool MonitorManagerImpl::setLaptopBrightness(int brightness) {
    if (!ensureWMIConnection()) {
        LOG_CRITICAL("MonitorManager",
                     "WMI connection unavailable for laptop brightness control");
        return false;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hres = pWMIService->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM WmiMonitorBrightnessMethods"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);

    if (FAILED(hres)) {
        LOG_CRITICAL("MonitorManager",
                     QString("WMI brightness methods query failed: %1").arg(QString::number(hres, 16)));
        return false;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    bool success = false;

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(2000, 1, &pclsObj, &uReturn);
        if (uReturn == 0) break;

        // Get the method class
        IWbemClassObject* pClass = NULL;
        hres = pWMIService->GetObject(bstr_t(L"WmiMonitorBrightnessMethods"), 0, NULL, &pClass, NULL);
        if (SUCCEEDED(hres)) {
            IWbemClassObject* pInParamsDefinition = NULL;
            hres = pClass->GetMethod(L"WmiSetBrightness", 0, &pInParamsDefinition, NULL);
            if (SUCCEEDED(hres)) {
                IWbemClassObject* pClassInstance = NULL;
                hres = pInParamsDefinition->SpawnInstance(0, &pClassInstance);

                if (SUCCEEDED(hres)) {
                    // Set parameters
                    VARIANT varBrightness, varTimeout;
                    VariantInit(&varBrightness);
                    VariantInit(&varTimeout);

                    varBrightness.vt = VT_UI1;
                    varBrightness.bVal = (BYTE)brightness;

                    varTimeout.vt = VT_I4;
                    varTimeout.lVal = 0;

                    HRESULT putBrightness = pClassInstance->Put(L"Brightness", 0, &varBrightness, 0);
                    HRESULT putTimeout = pClassInstance->Put(L"Timeout", 0, &varTimeout, 0);

                    if (SUCCEEDED(putBrightness)) {
                        // Execute method
                        VARIANT vtProp{};
                        VariantInit(&vtProp);
                        HRESULT pathResult = pclsObj->Get(L"__PATH", 0, &vtProp, 0, 0);

                        if (SUCCEEDED(pathResult) && vtProp.vt == VT_BSTR) {
                            IWbemClassObject* pOutParams = NULL;
                            hres = pWMIService->ExecMethod(vtProp.bstrVal, _bstr_t(L"WmiSetBrightness"),
                                                           0, NULL, pClassInstance, &pOutParams, NULL);

                            if (SUCCEEDED(hres)) {
                                success = true;
                            } else {
                                LOG_CRITICAL("MonitorManager",
                                             QString("WMI ExecMethod failed: %1").arg(QString::number(hres, 16)));
                            }

                            if (pOutParams) pOutParams->Release();
                        }
                        VariantClear(&vtProp);
                    }

                    VariantClear(&varBrightness);
                    VariantClear(&varTimeout);
                    pClassInstance->Release();
                }
                pInParamsDefinition->Release();
            }
            pClass->Release();
        }

        pclsObj->Release();
        break;
    }

    if (pEnumerator) pEnumerator->Release();
    return success;
}

int MonitorManagerImpl::getLaptopBrightness() {
    if (!ensureWMIConnection()) {
        return -1;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hres = pWMIService->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM WmiMonitorBrightness"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);

    if (FAILED(hres)) return -1;

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    int brightness = -1;

    if (pEnumerator) {
        HRESULT hr = pEnumerator->Next(2000, 1, &pclsObj, &uReturn);
        if (uReturn != 0) {
            VARIANT vtProp{};
            hr = pclsObj->Get(L"CurrentBrightness", 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_UI1)
            {
                brightness = vtProp.bVal;
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }
        pEnumerator->Release();
    }

    return brightness;
}

BOOL CALLBACK MonitorManagerImpl::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MonitorManagerImpl* manager = (MonitorManagerImpl*)dwData;

    MONITORINFOEXW monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFOEXW);
    GetMonitorInfoW(hMonitor, &monitorInfo);

    DWORD numPhysical;
    if (GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &numPhysical)) {
        PHYSICAL_MONITOR* physMonitors = new PHYSICAL_MONITOR[numPhysical];
        if (GetPhysicalMonitorsFromHMONITOR(hMonitor, numPhysical, physMonitors)) {
            for (DWORD i = 0; i < numPhysical; i++) {
                MonitorInfo info = {};
                info.physicalMonitor = physMonitors[i];
                info.deviceName = monitorInfo.szDevice;
                DISPLAY_DEVICEW displayDevice{};
                displayDevice.cb = sizeof(displayDevice);
                if (EnumDisplayDevicesW(monitorInfo.szDevice, i, &displayDevice, EDD_GET_DEVICE_INTERFACE_NAME) &&
                    displayDevice.DeviceID[0])
                    info.deviceId = displayDevice.DeviceID;
                else
                    info.deviceId = info.deviceName + L":" + std::to_wstring(i) + L":" +
                                    physMonitors[i].szPhysicalMonitorDescription;
                info.ddcciTested = false;
                info.ddcciWorking = false;
                info.isLaptopDisplay = false;
                info.cachedBrightness = -1;

                manager->monitors.push_back(info);
            }
        }
        delete[] physMonitors;
    }
    return TRUE;
}

void MonitorManagerImpl::setupChangeDetection() {
    LOG_INFO("MonitorManager",
             "Setting up monitor change detection");

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"MonitorChangeDetector";
    RegisterClassW(&wc);

    messageWindow =
        CreateWindowW(L"MonitorChangeDetector", L"", 0, 0, 0, 0, 0, nullptr, NULL, GetModuleHandle(NULL), this);
    SetWindowLongPtr(messageWindow, GWLP_USERDATA, (LONG_PTR)this);

    if (messageWindow) {
        LOG_INFO("MonitorManager",
                 "Monitor change detection window created successfully");
    } else {
        LOG_CRITICAL("MonitorManager",
                     "Failed to create monitor change detection window");
    }
}

LRESULT CALLBACK MonitorManagerImpl::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DISPLAYCHANGE) {
        MonitorManagerImpl* manager = (MonitorManagerImpl*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (manager) {
            LOG_INFO("MonitorManager",
                     "Display configuration changed, re-enumerating monitors");
            if (manager->changeCallback) {
                manager->changeCallback();
            }
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MonitorManagerImpl::cleanup() {
    for (auto& monitor : monitors) {
        if (!monitor.isLaptopDisplay) {
            DestroyPhysicalMonitor(monitor.physicalMonitor.hPhysicalMonitor);
        }
    }
    monitors.clear();
}

// Night Light implementation
void MonitorManagerImpl::initNightLightRegistry() {
    LOG_INFO("MonitorManager",
             "Initializing Night Light registry access");

    const std::wstring keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\CloudStore\\Store\\DefaultAccount\\Current\\default$windows.data.bluelightreduction.bluelightreductionstate\\windows.data.bluelightreduction.bluelightreductionstate";
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ | KEY_WRITE, &m_nightLightRegKey);
    if (result != ERROR_SUCCESS) {
        m_nightLightRegKey = nullptr;
        LOG_WARN("MonitorManager",
                 "Night Light registry key not accessible - feature not supported on this system");
    } else {
        LOG_INFO("MonitorManager",
                 "Night Light registry access initialized successfully");
    }
}

void MonitorManagerImpl::cleanupNightLightRegistry() {
    if (m_nightLightRegKey) {
        RegCloseKey(m_nightLightRegKey);
        m_nightLightRegKey = nullptr;
        LOG_INFO("MonitorManager",
                 "Night Light registry access cleaned up");
    }
}

std::vector<BYTE> MonitorManagerImpl::readNightLightData() const
{
    if (!m_nightLightRegKey)
        return {};
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueEx(m_nightLightRegKey, L"Data", nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_BINARY || (size != 41 && size != 43))
        return {};
    std::vector<BYTE> data(size);
    if (RegQueryValueEx(m_nightLightRegKey, L"Data", nullptr, &type, data.data(), &size) != ERROR_SUCCESS ||
        type != REG_BINARY || size != data.size())
        return {};
    return data;
}

bool MonitorManagerImpl::isNightLightSupported() {
    return NightLightData::enabled(readNightLightData()).has_value();
}

bool MonitorManagerImpl::isNightLightEnabled() {
    return NightLightData::enabled(readNightLightData()).value_or(false);
}

void MonitorManagerImpl::enableNightLight() {
    LOG_INFO("MonitorManager", "Enabling Night Light");
    if (isNightLightSupported() && !isNightLightEnabled()) {
        toggleNightLight();
    }
}

void MonitorManagerImpl::disableNightLight() {
    LOG_INFO("MonitorManager", "Disabling Night Light");
    if (isNightLightSupported() && isNightLightEnabled()) {
        toggleNightLight();
    }
}

void MonitorManagerImpl::toggleNightLight() {
    const auto data = NightLightData::toggle(readNightLightData());
    if (!data)
    {
        LOG_WARN("MonitorManager", "Unrecognized Night Light registry layout; refusing to modify it");
        return;
    }
    const LONG result =
        RegSetValueEx(m_nightLightRegKey, L"Data", 0, REG_BINARY, data->data(), static_cast<DWORD>(data->size()));
    if (result != ERROR_SUCCESS)
        LOG_WARN("MonitorManager", QString("Night Light write failed: %1").arg(result));
    }

std::wstring MonitorManagerImpl::getMonitorId(int index) const
{
    if (index < 0 || index >= getMonitorCount())
        return {};
    return monitors[index].deviceId;
    }

bool MonitorManagerImpl::isLaptopDisplay(int index) const
{
    return index >= 0 && index < getMonitorCount() && monitors[index].isLaptopDisplay;
        }
