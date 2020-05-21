import os
import re
import shutil
import sys
from shlex import quote

from tito.builder import Builder
from tito.common import (
    BUILDCONFIG_SECTION,
    debug,
    run_command,
    find_spec_like_file,
    get_spec_version_and_release,
    info_out,
    increase_version,
    increase_zstream,
    reset_release,
)
from tito.exception import TitoException
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

    def _copy_extra_sources(self):
        pass


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
        """
        We track package metadata in the .tito/packages/ directory. Each
        file here stores the latest package version (for the git branch you
        are on) as well as the relative path to the project's code. (from the
        git root)
        """
        self._clear_package_metadata()

        # Write out our package metadata:
        metadata_file = os.path.join(self.rel_eng_dir, "packages", self.project_name)

        with open(metadata_file, 'w') as f:
            f.write("%s %s\n" % (new_version.split('-')[0], self.relative_project_dir))

        # Git add it (in case it's a new file):
        run_command("git add %s" % metadata_file)
        run_command("git add %s" % os.path.join(self.full_project_dir,
            self.spec_file_name))
        run_command("git add %s" % self.fedora_spec_file)

        fmt = 'Automatic commit of package [%(name)s] %(release_type)s [%(version)s].'
        if self.config.has_option(BUILDCONFIG_SECTION, "tag_commit_message_format"):
            fmt = self.config.get(BUILDCONFIG_SECTION, "tag_commit_message_format")
        new_version_w_suffix = self._get_suffixed_version(new_version)
        try:
            msg = fmt % {
                'name': self.project_name,
                'release_type': self.release_type(),
                'version': new_version_w_suffix,
            }
        except KeyError:
            exc = sys.exc_info()[1]
            raise TitoException('Unknown placeholder %s in tag_commit_message_format'
                                % exc)

        run_command('git commit -m {0} -m {1} -m {2}'.format(
            quote(msg), quote("Created by command:"), quote(" ".join(sys.argv[:]))))

        new_tag = self._get_new_tag(new_version)
        tag_msg = "Tagging package [{}] version [{}] in directory [{}].".format(
            self.project_name, new_tag, self.relative_project_dir)

        # Optionally gpg sign the tag
        sign_tag = ""
        if self.config.has_option(BUILDCONFIG_SECTION, "sign_tag"):
            if self.config.getboolean(BUILDCONFIG_SECTION, "sign_tag"):
                sign_tag = "-s "

        run_command('git tag %s -m "%s" %s' % (sign_tag, tag_msg, new_tag))
        print()
        info_out("Created tag: %s" % new_tag)
        print("   View: git show HEAD")
        print("   Undo: tito tag -u")
        print("   Push: git push --follow-tags origin")
