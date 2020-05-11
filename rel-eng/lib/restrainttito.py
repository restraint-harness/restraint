import os
import re
import shutil

from tito.builder import Builder
from tito.common import (
    debug,
    run_command,
    find_spec_like_file,
    get_spec_version_and_release,
    increase_version,
    increase_zstream,
    reset_release
)
from tito.tagger import VersionTagger


class RestraintBuilder(Builder):

    def tgz(self):
        print('Fetching third-party tarballs')
        run_command('make -C third-party tarballs')
        debug('Copying third-party tarballs')
        self._create_build_dirs()
        for line in open('third-party/tarballs'):
            tarball = line.strip()
            assert os.path.isdir(self.rpmbuild_sourcedir)
            shutil.copy(os.path.join('third-party', tarball), self.rpmbuild_sourcedir)
            self.sources.append(os.path.join(self.rpmbuild_sourcedir, tarball))
        return super(RestraintBuilder, self).tgz()


class RestraintVersionTagger(VersionTagger):

    def __init__(self, config=None, keep_version=False, offline=False, user_config=None):
        super(RestraintVersionTagger, self).__init__(config, keep_version, offline, user_config)
        fedora_spec_file_name = find_spec_like_file(os.path.join(os.getcwd(), 'specfiles/'))
        self.fedora_spec_file = os.path.join(self.full_project_dir, 'specfiles/', fedora_spec_file_name)

    def _bump_version(self, release=False, zstream=False):
        """
        Bump version in main and Fedora spec file

        Version and Release in Fedora spec file are always updated accordingly to main spec file
        """

        # Version and Release without dist tag
        old_version, old_release = get_spec_version_and_release(self.full_project_dir, self.spec_file_name).split('-')
        old_release += '%{?dist}'
        new_version = super(RestraintVersionTagger, self)._bump_version(release, zstream)
        if not self.keep_version:
            version_regex = re.compile("^(version:\s*)(.+)$", re.IGNORECASE)
            release_regex = re.compile("^(release:\s*)(.+)$", re.IGNORECASE)

            in_f = open(self.fedora_spec_file, 'r')
            out_f = open(self.fedora_spec_file + ".new", 'w')

            for line in in_f.readlines():
                version_match = re.match(version_regex, line)
                release_match = re.match(release_regex, line)

                if version_match and not zstream and not release:
                    if hasattr(self, '_use_version'):
                        updated_content = self._use_version
                    else:
                        updated_content = increase_version(old_version)

                    line = "".join([version_match.group(1), updated_content, "\n"])

                elif release_match:
                    if hasattr(self, '_use_release'):
                        updated_content = self._use_release
                    elif release:
                        updated_content = increase_version(old_release)
                    elif zstream:
                        updated_content = increase_zstream(old_release)
                    else:
                        updated_content = reset_release(old_release)

                    line = "".join([release_match.group(1), updated_content, "\n"])

                out_f.write(line)

            in_f.close()
            out_f.close()
            shutil.move(self.fedora_spec_file + ".new", self.fedora_spec_file)

        return new_version

    def _update_package_metadata(self, new_version):
        run_command("git add %s" % self.fedora_spec_file)
        return super(RestraintVersionTagger, self)._update_package_metadata(new_version)
