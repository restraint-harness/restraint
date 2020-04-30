import os
import shutil

from tito.builder import Builder
from tito.common import debug, run_command


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
