"""Actual Java save implementation self-check; originals and device stay read-only."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='saves-check-', dir=ROOT / 'analysis/research') as temporary:
    work = Path(temporary)
    subprocess.run(['javac', '-d', str(work), str(ROOT / 'analysis/android/com/epicgames/ue4/MobileSaves.java'), str(ROOT / 'analysis/research/MobileSavesTest.java')], check=True)
    subprocess.run(['java', '-ea', '-cp', str(work), 'MobileSavesTest', str(work / 'files'),
                    str(ROOT / 'analysis/research/p270_save_backup/SaveGames/System.sav'),
                    str(ROOT / 'analysis/research/p270_save_backup/savegame00.sav')], check=True)
