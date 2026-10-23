#!/usr/bin/env python3
# shared: sdk/scripts/framelog.py
import os, runpy, sys
here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sdk", "scripts", "framelog.py")
sys.argv[0] = here
runpy.run_path(here, run_name="__main__")
