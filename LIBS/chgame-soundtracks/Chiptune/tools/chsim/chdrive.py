"""Drive the demo through the shared CHGame simulator/device protocol."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'CHGame/tools/chsim'))
from chdrivelib import Driver, main

if __name__ == '__main__':
    main(Driver, ident='CHIP')
