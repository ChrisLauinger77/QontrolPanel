## Translate QontrolPanel

### Install Qt linguist

Download Qt Linguist [here](https://github.com/thurask/Qt-Linguist/releases) (64-bit Windows Version)<br>
Unzip it and run `linguist.exe` from inside the unzipped folder.<br>
More information can be found here: [Qt linguist](https://doc.qt.io/qt-6/linguist-translators.html).

### New languages

For new languages we also need to add a new line in [cmake/languages.cmake](../cmake/languages.cmake).

### Testing your changes

You can override the qm files in your QontrolPanel install (default: `%localappdata%\programs\QontrolPanel\bin\i18n`) with `file` -> `release as...`

### Validating your changes

Once satisfied with your changes, only commit the `.ts` file.  
On changes merged, a workflow will handle the compiled translation generation.

The generated `.qm` files and `translation_progress.json` are committed back into this repository under `i18n_compiled/` by CI/CD.

Users will be able to update translation within the program with those newly generated ones.
