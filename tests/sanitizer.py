#!/usr/bin/env python3
# Checks the reports of a sanitizer build (tests/run.sh -Db_sanitize=...):
# every process of the plug-in writes tests/output/sanitizer.<pid>.
#
# Any AddressSanitizer or UndefinedBehaviorSanitizer error fails. Of the
# leaks, only those allocated from the plug-in's own code count: libgimp
# does not free the plug-in and procedure objects before the process exits,
# and gegl_init() loads modules that keep their memory, so allocations
# made through gegl_init() or the create_procedure() method are left out.
import glob
import re
import sys
import time

out = sys.argv[1]

# the plug-in processes may still be writing when GIMP has quit: wait
# until every report is complete
time.sleep(2)
for _ in range(60):
    reports = sorted(glob.glob(out + '/sanitizer.*'))
    if all('SUMMARY:' in open(p).read() for p in reports):
        break
    time.sleep(1)
errors, leaks, ignored = [], [], 0
for path in reports:
    text = open(path).read()
    for m in re.finditer(r'^.*(ERROR: AddressSanitizer|runtime error).*$',
                         text, re.M):
        errors.append('%s: %s' % (path, m.group(0)))
    for block in text.split('\n\n'):
        if not re.match(r'(Direct|Indirect) leak', block.strip()):
            continue
        frames = re.findall(r'#\d+ \S+ in (\S+) (\S+)', block)
        # (the sanitizer runtime's own frames are in .../src/libsanitizer)
        own = [f for f in frames
               if '/src/' in f[1] and '/libsanitizer/' not in f[1]]
        if not own or any(fn == 'gegl_init' or fn.endswith('_create_procedure')
                          for fn, _ in frames):
            ignored += 1
            continue
        leaks.append('%s:\n%s' % (path, block.strip()))

print('%d reports, %d leaks of libraries ignored' % (len(reports), ignored))
for e in errors:
    print(e)
for l in leaks:
    print(l)
# without any report (libgimp always leaves some), the sanitizer did not run
ok = reports and not errors and not leaks
print('%s sanitizer: %d errors, %d leaks of the plug-in'
      % ('PASS' if ok else 'FAIL', len(errors), len(leaks)))
sys.exit(0 if ok else 1)
