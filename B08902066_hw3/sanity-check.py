import re
import sys
from zipfile import ZipFile
from pathlib import Path

'''
B09902999_hw3.zip   <─── Replace "B09902999" with your student ID
└── B09902999_hw3   <────┘
    ├── <your source code>
    └── Makefile
'''

def is_sid_dir(dir):
    if re.match('^([BDR][0-9]{8}|[0-9]{8}E)\_hw3$', dir):
        return True
    else:
        return False

if len(sys.argv) < 2:
    print ('Usage: python3 sanity-check.py <path/to/target.zip>')
    exit(1)

zip_filepath = Path(sys.argv[1])
zip_file = ZipFile(zip_filepath)

dir_set = {d.split('/')[0] for d in zip_file.namelist()}
sid_dir = ''

for dir in dir_set:
    if is_sid_dir(dir):
        sid_dir = dir

if sid_dir != '' and sid_dir + '/Makefile' in zip_file.namelist():
    print('PASS!!')
else:
    print('Failed...')