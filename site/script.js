document.querySelectorAll("[data-copy]").forEach((button) => {
  button.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(button.dataset.copy);
      button.textContent = "Copied";
      window.setTimeout(() => { button.textContent = "Copy"; }, 2000);
    } catch {
      button.textContent = "Select text";
      window.setTimeout(() => { button.textContent = "Copy"; }, 2000);
    }
  });
});
