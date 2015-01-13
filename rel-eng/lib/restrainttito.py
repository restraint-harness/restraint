import re
import os
import sys
from tito.common import debug, run_command
from tito.builder import Builder, UpstreamBuilder

class RestraintBuilder(Builder):

    def tgz(self):
        self._setup_sources()

        run_command("cp %s/%s %s/" %  \
                (self.rpmbuild_sourcedir, self.tgz_filename,
                    self.rpmbuild_basedir))

        self.ran_tgz = True
        full_path = os.path.join(self.rpmbuild_basedir, self.tgz_filename)
        print("Wrote: %s" % full_path)
        self.sources.append(full_path)
        self.artifacts.append(full_path)

        run_command("pushd %s/third-party && make tarballs && popd" % self.rpmbuild_gitcopy)
        run_command("pushd %s && tar zcf %s %s && popd" % (self.rpmbuild_sourcedir,
                                                           self.tgz_filename, self._get_tgz_name_and_ver()))
	run_command("cp %s/%s %s/" %  \
                (self.rpmbuild_sourcedir, self.tgz_filename,
                    self.rpmbuild_basedir))
                                                           
        return full_path
