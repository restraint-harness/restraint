import os
import sys
import shutil
from tito.common import debug, run_command
from tito.builder import Builder

class RestraintBuilder(Builder):

    def tgz(self):
        print('Fetching third-party tarballs')
        run_command('make -C third-party tarballs')
        debug('Copying third-party tarballs')
        for line in open('third-party/tarballs'):
            tarball = line.strip()
            shutil.copy(os.path.join('third-party', tarball), self.rpmbuild_sourcedir)
            self.sources.append(tarball)
        return super(RestraintBuilder, self).tgz()
