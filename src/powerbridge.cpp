#include "powerbridge.h"
#include "logmanager.h"
#include "workerthreads.h"
#include <windows.h>
#include <shellapi.h>
#include <lm.h>
#include <powrprof.h>
#include <string>

namespace
{
    bool enableShutdownPrivilege()
    {
        HANDLE token = nullptr;
        TOKEN_PRIVILEGES privileges{};
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
            return false;
        if (!LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid))
        {
            const DWORD error = GetLastError();
            CloseHandle(token);
            SetLastError(error);
            return false;
        }
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        SetLastError(ERROR_SUCCESS);
        const BOOL adjusted = AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
        const DWORD error = GetLastError();
        CloseHandle(token);
        SetLastError(error);
        return adjusted && error == ERROR_SUCCESS;
    }
} // namespace

void PowerWorker::refreshCapabilities()
{
    SYSTEM_POWER_CAPABILITIES power{};
    const bool available = GetPwrCapabilities(&power);
    FIRMWARE_TYPE firmware = FirmwareTypeUnknown;
    GetFirmwareType(&firmware);
    DWORD users = 0;
    DWORD resume = 0;
    NET_API_STATUS status;
    do
    {
        LPUSER_INFO_1 buffer = nullptr;
        DWORD entries = 0, total = 0;
        status = NetUserEnum(nullptr, 1, FILTER_NORMAL_ACCOUNT, reinterpret_cast<LPBYTE*>(&buffer), 64 * 1024, &entries,
                             &total, &resume);
        if (status == NERR_Success || status == ERROR_MORE_DATA)
        {
            for (DWORD i = 0; i < entries; ++i)
            {
                const QString name = QString::fromWCharArray(buffer[i].usri1_name);
                if (!(buffer[i].usri1_flags & UF_ACCOUNTDISABLE) && !name.startsWith('_'))
                    ++users;
            }
        }
        if (buffer)
            NetApiBufferFree(buffer);
    } while (status == ERROR_MORE_DATA && users < 2);
    emit capabilitiesReady({{"sleep", available && (power.AoAc || power.SystemS1 || power.SystemS2 || power.SystemS3)},
                            {"hibernate", available && power.SystemS4 && power.HiberFilePresent},
                            {"uefi", firmware == FirmwareTypeUefi},
                            {"users", users > 1}});
}

void PowerWorker::execute(const QString& action)
{
    bool success = false;
    SetLastError(ERROR_SUCCESS);
    if (action == "shutdown" || action == "restart")
    {
        success =
            enableShutdownPrivilege() && ExitWindowsEx(action == "shutdown" ? EWX_SHUTDOWN : EWX_REBOOT,
                                                       SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED);
    }
    else if (action == "sleep" || action == "hibernate")
    {
        success = enableShutdownPrivilege() && SetSuspendState(action == "hibernate", FALSE, FALSE);
    }
    else if (action == "signOut")
    {
        success = ExitWindowsEx(EWX_LOGOFF, SHTDN_REASON_MAJOR_APPLICATION);
    }
    else if (action == "lock" || action == "switchAccount")
    {
        success = LockWorkStation();
    }
    else if (action == "uefi")
    {
        // Resolve the system executable explicitly instead of searching PATH.
        wchar_t systemDirectory[MAX_PATH]{};
        if (GetSystemDirectoryW(systemDirectory, MAX_PATH))
        {
            const std::wstring executable = std::wstring(systemDirectory) + L"\\shutdown.exe";
            const auto result = reinterpret_cast<INT_PTR>(
                ShellExecuteW(nullptr, L"runas", executable.c_str(), L"/r /fw /t 0", nullptr, SW_HIDE));
            success = result > 32;
            if (!success)
                SetLastError(result == SE_ERR_ACCESSDENIED ? ERROR_CANCELLED : ERROR_GEN_FAILURE);
        }
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    const DWORD error = success ? ERROR_SUCCESS : (GetLastError() ? GetLastError() : ERROR_GEN_FAILURE);
    emit completed(action, success, error);
}

PowerBridge* PowerBridge::m_instance = nullptr;
PowerBridge::PowerBridge(QObject* parent) : QObject(parent)
{
    m_instance = this;
    m_thread = new QThread();
    m_worker = new PowerWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &PowerWorker::refreshCapabilities);
    connect(m_worker, &PowerWorker::capabilitiesReady, this, [this](const QVariantMap& capabilities) {
        m_capabilities = capabilities;
        emit capabilitiesChanged();
    });
    connect(m_worker, &PowerWorker::completed, this, [this](const QString& action, bool success, unsigned long error) {
        m_busy = false;
        emit busyChanged();
        if (!success)
        {
            LOG_WARN("PowerManager", QString("Power action %1 failed: %2").arg(action).arg(error));
            m_lastError = tr("Windows could not complete the power action (error %1).").arg(error);
            emit lastErrorChanged();
            emit operationFailed(m_lastError);
}
    });
    m_thread->start();
}
PowerBridge::~PowerBridge()
{
    disconnect(m_worker, nullptr, this, nullptr);
    retireWorkerThread(m_thread, m_worker, "cleanup");
        m_instance = nullptr;
    }
PowerBridge* PowerBridge::create(QQmlEngine*, QJSEngine*)
{
    if (!m_instance)
        m_instance = new PowerBridge();
    return m_instance;
}
PowerBridge* PowerBridge::instance()
{
    return m_instance;
}
bool PowerBridge::isSleepSupported()
{
    return m_capabilities.value("sleep").toBool();
}
bool PowerBridge::isHibernateSupported()
{
    return m_capabilities.value("hibernate").toBool();
}
bool PowerBridge::isUEFISupported()
{
    return m_capabilities.value("uefi").toBool();
}
bool PowerBridge::hasMultipleUsers()
{
    return m_capabilities.value("users").toBool();
    }

bool PowerBridge::enqueue(const QString& action)
{
    if (m_busy)
        return false;
    m_busy = true;
    m_lastError.clear();
    emit busyChanged();
    emit lastErrorChanged();
    QMetaObject::invokeMethod(m_worker, "execute", Qt::QueuedConnection, Q_ARG(QString, action));
    return true;
}
bool PowerBridge::shutdown()
{
    return enqueue("shutdown");
            }
bool PowerBridge::restart()
{
    return enqueue("restart");
        }
bool PowerBridge::sleep()
{
    return enqueue("sleep");
    }
bool PowerBridge::hibernate()
{
    return enqueue("hibernate");
}
bool PowerBridge::lockAccount()
{
    return enqueue("lock");
    }
bool PowerBridge::signOut()
{
    return enqueue("signOut");
}
bool PowerBridge::switchAccount()
{
    return enqueue("switchAccount");
    }
void PowerBridge::restartToUEFI()
{
    enqueue("uefi");
    }
