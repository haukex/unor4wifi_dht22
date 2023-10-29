#!/bin/bash
set -euxo pipefail
cd "$( dirname "${BASH_SOURCE[0]}" )"

cd webpage
rm -rf dist/
npm test
npm run build
cd ..

python3 make_favicon.py
python3 make_write_html.py

python3 ardu.py upload
