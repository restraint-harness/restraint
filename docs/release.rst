Release Guide
=============

This document describes Restraint Release process and all necessary steps.
Currently, it is not possible to perform release outside of Red Hat internal
network as release depends on many internal services.

Tools
-----

Following packages should be installed in a given system:

- tito
- koji
- reno
- rhpkg
- brewkoji

Steps
-----
1. Create new release notes with Reno. Content should be appended on top of current Release notes.

2. Create new review in GitHub for Release Notes.

3. When review is done pull all the changes from remote.

4. Tag new release with ``tito tag``.

5. Push all tags to remote.

6. Use tito to release Restraint. It can be done by running ``tito release dist-git``. All current chroots are listed in ``rel-eng/releasers.conf``.

7. When all builds are finished move them from -candidates tags to proper one. For example ``brew move build eng-fedora-<ver>-candidate eng-fedora-<ver> restraint-<version>.fc<ver>eng``.

8. After all builds are moved from candidate, builds are automatically downloaded and stored under https://beaker-project.org/yum/harness/.

9. Done!

