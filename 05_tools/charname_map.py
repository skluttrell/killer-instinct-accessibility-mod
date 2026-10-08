"""Build the fighter codename -> display name map from logged MainMenu PopulatePlayerCards payloads
(each entry pairs PlayerCard_Expanded_Char_Images/<codename>.dds with "Title":"&<Display> - Lvl N").
Usage: python -I charname_map.py <log> [<log> ...]   -> prints a JSON dict and writes data/fighter_names.json
"""
import json
import os
import re
import sys

pairs = {}
for path in sys.argv[1:]:
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "PopulatePlayerCards" not in line:
                continue
            for m in re.finditer(r'PlayerCard_Expanded_Char_Images/(\w+)\.dds(?:\\"|"),(?:\\"|")Title(?:\\"|"):(?:\\"|")&([^"\\]+?) - Lvl', line):
                pairs[m.group(1)] = m.group(2)
out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "fighter_names.json")
if pairs:
    with open(out, "w", encoding="utf-8") as f:
        json.dump(dict(sorted(pairs.items())), f, indent=1)
print(json.dumps(dict(sorted(pairs.items())), indent=1))
print(len(pairs), "names ->", out)
