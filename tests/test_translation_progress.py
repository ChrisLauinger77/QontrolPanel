import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class TranslationProgressTests(unittest.TestCase):
    previous_date = '2026-11-01 18:14:23'
    commit_date = '2026-11-01 18:51:47'
    later_date = '2026-11-02 10:00:00'

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / 'repo'
        self.compiled = self.repo / 'i18n_compiled'
        self.staging = Path(self.temp.name) / 'generated'
        for directory in (self.repo / 'tools', self.repo / 'i18n', self.compiled, self.staging):
            directory.mkdir(parents=True)
        shutil.copyfile(Path(__file__).resolve().parents[1] / 'tools/analyze_translations.py',
                        self.repo / 'tools/analyze_translations.py')
        self.previous = {}
        for language in ('en', 'de'):
            self.write_ts(language)
            qm_name = f'QontrolPanel_{language}.qm'
            for directory in (self.compiled, self.staging):
                (directory / qm_name).write_bytes(f'compiled {language}'.encode())
            self.previous[language] = {
                'percentage': 100,
                'last_updated': self.previous_date,
                'contributor': 'ChrisLauinger77' if language == 'en' else None,
            }
        # Saved metadata order deliberately differs from source creation order.
        self.previous = dict(reversed(self.previous.items()))
        (self.compiled / 'translation_progress.json').write_text(
            json.dumps(self.previous, indent=2), encoding='utf-8')
        self.git('init', '--quiet')
        self.git('config', 'user.name', 'Translation Test')
        self.git('config', 'user.email', 'translation-test@example.com')
        self.git('config', 'commit.gpgsign', 'false')
        self.commit(self.commit_date)

    def git(self, *arguments, date=None):
        environment = os.environ.copy()
        if date:
            environment.update(GIT_AUTHOR_DATE=f'{date} +0000', GIT_COMMITTER_DATE=f'{date} +0000')
        result = subprocess.run(['git', *arguments], cwd=self.repo, env=environment,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def commit(self, date):
        self.git('add', '.')
        self.git('commit', '--quiet', '-m', 'Update translations', date=date)

    def write_ts(self, language, unfinished=False):
        translation = '<translation type="unfinished"/>' if unfinished else '<translation>Test</translation>'
        (self.repo / 'i18n' / f'QontrolPanel_{language}.ts').write_text(
            f'<TS language="{language}"><context><name>Test</name>'
            f'<message><source>Test</source>{translation}</message></context></TS>', encoding='utf-8')

    def generate(self):
        result = subprocess.run(
            [sys.executable, str(self.repo / 'tools/analyze_translations.py'),
             '--compiled-dir', str(self.staging)],
            cwd=self.repo, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads((self.staging / 'translation_progress.json').read_text(encoding='utf-8'))

    def test_already_compiled_squash_merge_preserves_progress(self):
        # The merge commit is newer than the artifacts generated on its branch.
        self.assertEqual(self.generate(), self.previous)
        first_output = (self.staging / 'translation_progress.json').read_bytes()
        self.assertEqual(first_output, (self.compiled / 'translation_progress.json').read_bytes())
        self.assertEqual(self.generate(), self.previous)
        self.assertEqual((self.staging / 'translation_progress.json').read_bytes(), first_output)
        self.assertFalse((self.repo / 'i18n/compiled').exists())

    def test_source_only_english_change_preserves_date(self):
        with (self.repo / 'i18n/QontrolPanel_en.ts').open('a', encoding='utf-8') as source:
            source.write('\n<!-- Extraction changed without altering compiled messages. -->\n')
        self.commit(self.later_date)
        self.assertEqual(self.generate(), self.previous)

    def test_changed_english_catalog_updates_date(self):
        (self.staging / 'QontrolPanel_en.qm').write_bytes(b'new compiled English messages')
        progress = self.generate()
        self.assertEqual(progress['en']['last_updated'], self.commit_date)
        self.assertEqual(progress['en']['percentage'], 100)
        self.assertEqual(progress['de'], self.previous['de'])

    def test_missing_previous_catalog_uses_source_date(self):
        (self.compiled / 'QontrolPanel_en.qm').unlink()
        self.assertEqual(self.generate()['en']['last_updated'], self.commit_date)

    def test_missing_previous_progress_uses_source_date(self):
        (self.compiled / 'translation_progress.json').unlink()
        self.assertEqual(self.generate()['en']['last_updated'], self.commit_date)

    def test_single_language_translation_still_updates(self):
        self.write_ts('de', unfinished=True)
        self.commit(self.later_date)
        progress = self.generate()
        self.assertEqual(progress['en'], self.previous['en'])
        self.assertEqual(progress['de']['last_updated'], self.later_date)
        self.assertEqual(progress['de']['percentage'], 0)
        self.assertEqual(progress['de']['contributor'], 'Translation Test')


if __name__ == '__main__':
    unittest.main()
