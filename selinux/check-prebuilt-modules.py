#!/usr/bin/python3
"""Check that prebuilt SELinux modules load on the oldest minor release.

restraint.spec installs a prebuilt module, such as restraint.el10.pp, when
one matches %{dist}. A module built with a newer SELinux toolchain uses a
newer module format, and older releases refuse to load it:

  libsepol.policydb_read: policydb module version 24 does not match my
  version range 4-22

That leaves restraint running without its policy. This script reads the
module format of every prebuilt module and compares it with the highest
format that the .0 release of that RHEL major version accepts.

To rebuild a module, use a host or container on the .0 release:

  make -f /usr/share/selinux/devel/Makefile restraint.pp
  cp restraint.pp restraint.el<N>.pp
"""

import glob
import os
import re
import struct
import sys

# Highest module format accepted by the .0 release of each RHEL major
# version, as printed by "checkmodule -V" on that release.
MAX_MODULE_FORMAT = {
    6: 10,   # RHEL 6.0, checkpolicy 2.0.22
    7: 17,   # RHEL 7.0, checkpolicy 2.1.12
    9: 20,   # RHEL 9.0, checkpolicy 3.3
    10: 22,  # RHEL 10.0, checkpolicy 3.8
}

PKG_MAGIC = 0xF97CFF8F
MOD_MAGIC = 0xF97CFF8D
POLICY_MOD = 2

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PATTERNS = ("selinux/*.el*.pp", "legacy/selinux/*.el*.pp")


def module_format(path):
    """Return the module format version stored in a .pp policy package."""
    with open(path, "rb") as f:
        data = f.read()
    magic, _, sections = struct.unpack_from("<III", data, 0)
    if magic != PKG_MAGIC or sections < 1:
        raise ValueError("not a SELinux policy package")
    offset = struct.unpack_from("<I", data, 12)[0]
    magic, length = struct.unpack_from("<II", data, offset)
    if magic != MOD_MAGIC:
        raise ValueError("first section is not a policy module")
    policy_type, version = struct.unpack_from("<II", data, offset + 8 + length)
    if policy_type != POLICY_MOD:
        raise ValueError("policy type %d is not a module" % policy_type)
    return version


def main():
    failed = False
    paths = sorted(p for pattern in PATTERNS
                   for p in glob.glob(os.path.join(ROOT, pattern)))
    for path in paths:
        name = os.path.relpath(path, ROOT)
        match = re.search(r"\.el(\d+)[a-z]*\.pp$", path)
        if not match:
            continue
        major = int(match.group(1))
        try:
            version = module_format(path)
        except (ValueError, struct.error) as e:
            print("FAIL %s: %s" % (name, e))
            failed = True
            continue
        limit = MAX_MODULE_FORMAT.get(major)
        if limit is None:
            print("FAIL %s: format %d, but RHEL %d is not in MAX_MODULE_FORMAT"
                  % (name, version, major))
            failed = True
        elif version > limit:
            print("FAIL %s: format %d, but RHEL %d.0 accepts at most %d"
                  % (name, version, major, limit))
            failed = True
        else:
            print("ok   %s: format %d (RHEL %d.0 accepts at most %d)"
                  % (name, version, major, limit))
    if not paths:
        print("FAIL no prebuilt modules found")
        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
