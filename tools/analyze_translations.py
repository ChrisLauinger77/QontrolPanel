#!/usr/bin/env python3
import json
import xml.etree.ElementTree as ET
from pathlib import Path
import sys
import subprocess
from datetime import datetime
import io

# Ensure UTF-8 output on Windows
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

def analyze_ts_file(ts_file_path):
    try:
        tree = ET.parse(ts_file_path)
        root = tree.getroot()
       
        total_messages = 0
        unfinished_messages = 0
       
        for message in root.findall('.//message'):
            translation = message.find('translation')
            if translation is None:
                continue
               
            translation_type = translation.get('type', '').lower()
           
            if translation_type == 'vanished':
                continue
               
            total_messages += 1
           
            if translation_type == 'unfinished':
                unfinished_messages += 1
                   
        finished_messages = total_messages - unfinished_messages
       
        if total_messages == 0:
            percentage = 0
        else:
            percentage = int((finished_messages / total_messages) * 100)
           
        return {
            'total_messages': total_messages,
            'translated_messages': finished_messages,
            'percentage': percentage
        }
       
    except ET.ParseError as e:
        print(f"Error parsing {ts_file_path}: {e}")
        return None
    except Exception as e:
        print(f"Error processing {ts_file_path}: {e}")
        return None

def extract_language_code(filename):
    if filename.startswith('QontrolPanel_') and filename.endswith('.ts'):
        return filename[len('QontrolPanel_'):-len('.ts')]

    return filename

def get_last_commit_info(file_path):
    """Get the last commit hash and date for a specific file"""
    try:
        result = subprocess.run(
            ['git', 'log', '-1', '--format=%H %ci', '--', str(file_path)],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            cwd=file_path.parent.parent
        )
        if result.returncode == 0 and result.stdout.strip():
            parts = result.stdout.strip().split(' ', 1)
            commit_hash = parts[0]
            date_str = parts[1] if len(parts) > 1 else None
            return commit_hash, date_str
        return None, None
    except Exception as e:
        print(f"  Warning: Could not get git commit info: {e}")
        return None, None

def get_all_contributors(file_path, i18n_dir):
    """Get the contributor who made the most single-file commits to this file"""
    def get_contributor_with_follow(use_follow):
        """Helper function to get contributor with or without --follow"""
        try:
            # Get all commits that touched this file with hash and author
            cmd = ['git', 'log', '--format=%H|%an']
            if use_follow:
                cmd.append('--follow')
            cmd.extend(['--', str(file_path)])

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                cwd=i18n_dir.parent
            )
            if result.returncode != 0 or not result.stdout or not result.stdout.strip():
                return None

            # Count commits per author (only single-file commits)
            author_commits = {}
            for line in result.stdout.strip().split('\n'):
                if not line or '|' not in line:
                    continue

                parts = line.split('|', 1)
                if len(parts) != 2:
                    continue

                commit_hash = parts[0].strip()
                author = parts[1].strip()

                if not commit_hash or not author:
                    continue

                # Only count if this commit had only one .ts file
                ts_count = count_ts_files_in_commit(commit_hash, i18n_dir)
                if ts_count == 1:
                    author_commits[author] = author_commits.get(author, 0) + 1

            # Return the author with the most commits
            if author_commits:
                top_contributor = max(author_commits.items(), key=lambda x: x[1])
                return top_contributor[0]

            return None
        except Exception as e:
            print(f"  Warning: Could not get contributors: {e}", flush=True)
            return None

    # Try without --follow first (current file history only)
    contributor = get_contributor_with_follow(use_follow=False)

    # If no contributor found, try with --follow (includes renames/history)
    if not contributor:
        print(f"  No contributor found in current history, checking full history...", flush=True)
        contributor = get_contributor_with_follow(use_follow=True)

    return contributor

def count_ts_files_in_commit(commit_hash, i18n_dir):
    """Count how many .ts files were modified in a given commit"""
    try:
        result = subprocess.run(
            ['git', 'show', '--name-only', '--format=', commit_hash],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            cwd=i18n_dir.parent
        )
        if result.returncode == 0:
            modified_files = result.stdout.strip().split('\n')
            ts_count = sum(1 for f in modified_files if f.startswith('i18n/') and f.endswith('.ts'))
            return ts_count
        return 0
    except Exception as e:
        print(f"  Warning: Could not count files in commit: {e}")
        return 0

def find_last_single_file_commit(file_path, i18n_dir):
    """Find the most recent commit that ONLY modified this specific .ts file"""
    def search_with_follow(use_follow):
        """Helper to search with or without --follow"""
        try:
            # Get all commits that touched this file
            cmd = ['git', 'log', '--format=%H']
            if use_follow:
                cmd.append('--follow')
            cmd.extend(['--', str(file_path)])

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding='utf-8',
                errors='replace',
                cwd=i18n_dir.parent
            )
            if result.returncode != 0 or not result.stdout.strip():
                return None, None

            commit_hashes = result.stdout.strip().split('\n')

            # Search through commits to find one with only this .ts file
            for commit_hash in commit_hashes:
                ts_count = count_ts_files_in_commit(commit_hash, i18n_dir)
                if ts_count == 1:
                    # Found a commit with only this file, get its date
                    date_result = subprocess.run(
                        ['git', 'show', '--format=%ci', '--no-patch', commit_hash],
                        capture_output=True,
                        text=True,
                        encoding='utf-8',
                        errors='replace',
                        cwd=i18n_dir.parent
                    )
                    if date_result.returncode == 0 and date_result.stdout.strip():
                        commit_date = date_result.stdout.strip()
                        history_type = "full history" if use_follow else "current history"
                        print(f"  Found single-file commit in {history_type}: {commit_hash[:7]} on {commit_date}", flush=True)
                        return commit_hash, commit_date

            return None, None
        except Exception as e:
            print(f"  Warning: Could not search commit history: {e}", flush=True)
            return None, None

    # Try without --follow first (current file history only)
    commit_hash, commit_date = search_with_follow(use_follow=False)

    # If not found, try with --follow (includes renames/history)
    if not commit_hash:
        print(f"  No single-file commit in current history, checking full history...", flush=True)
        commit_hash, commit_date = search_with_follow(use_follow=True)

    if not commit_hash:
        print(f"  No single-file commit found in any history", flush=True)

    return commit_hash, commit_date

def main():
    script_dir = Path(__file__).parent
    i18n_dir = script_dir.parent / 'i18n'
    compiled_dir = i18n_dir / 'compiled'

    compiled_dir.mkdir(exist_ok=True)

    output_file = compiled_dir / 'translation_progress.json'

    # Load existing data to preserve dates when multiple files are updated
    existing_progress = {}
    if output_file.exists():
        try:
            with open(output_file, 'r', encoding='utf-8') as f:
                existing_progress = json.load(f)
            print(f"Loaded existing translation progress from {output_file}")
        except Exception as e:
            print(f"Warning: Could not load existing progress: {e}")

    print("Analyzing translation files...")
    print(f"Looking for .ts files in: {i18n_dir}")

    ts_files = list(i18n_dir.glob('*.ts'))

    if not ts_files:
        print("No .ts files found in i18n directory!")
        sys.exit(1)

    print(f"Found {len(ts_files)} translation files")

    translation_progress = {}
   
    for ts_file in ts_files:
        print(f"Analyzing {ts_file.name}...", flush=True)

        language_code = extract_language_code(ts_file.name)

        # Get the top contributor for this file
        contributor = get_all_contributors(ts_file, i18n_dir)

        # Get last commit info
        commit_hash, commit_date = get_last_commit_info(ts_file)

        # Determine if we should update the date
        should_update_date = False
        if commit_hash:
            ts_count_in_commit = count_ts_files_in_commit(commit_hash, i18n_dir)
            if ts_count_in_commit == 1:
                should_update_date = True
                print(f"  Single .ts file in commit {commit_hash[:7]} - will update date", flush=True)
            else:
                print(f"  {ts_count_in_commit} .ts files in commit {commit_hash[:7]} - preserving old date", flush=True)

        # Decide which date to use
        if should_update_date:
            last_updated = commit_date
        else:
            # Preserve old date if it exists
            old_data = existing_progress.get(language_code, {})
            last_updated = old_data.get('last_updated')
            if not last_updated:
                # If no old date exists, search git history for a single-file commit
                print(f"  No existing date found, searching git history...", flush=True)
                _, history_date = find_last_single_file_commit(ts_file, i18n_dir)
                last_updated = history_date

        # Special case for English - always 100%
        if language_code.lower() in ['en', 'english', 'en_us', 'en_gb']:
            # English is the source language - use current date and original author
            current_date = datetime.now().strftime('%Y-%m-%d %H:%M:%S %z')
            translation_progress[language_code] = {
                'percentage': 100,
                'last_updated': current_date,
                'contributor': 'ChrisLauinger77'
            }
            print(f"  {language_code}: 100% (English - source language)", flush=True)
            print(f"    Last updated: {current_date}", flush=True)
            print(f"    Contributor: ChrisLauinger77", flush=True)
            continue

        stats = analyze_ts_file(ts_file)
        if stats is None:
            print(f"Skipping {ts_file.name} due to errors", flush=True)
            continue

        translation_progress[language_code] = {
            'percentage': stats['percentage'],
            'last_updated': last_updated,
            'contributor': contributor
        }

        print(f"  {language_code}: {stats['translated_messages']}/{stats['total_messages']} = {stats['percentage']}%", flush=True)
        if last_updated:
            print(f"    Last updated: {last_updated}", flush=True)
        if contributor:
            print(f"    Contributor: {contributor}", flush=True)
   
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(translation_progress, f, indent=2, ensure_ascii=False)
       
        print(f"\nTranslation progress saved to: {output_file}")
        print("Contents:")
        print(json.dumps(translation_progress, indent=2))
       
    except Exception as e:
        print(f"Error saving JSON file: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()