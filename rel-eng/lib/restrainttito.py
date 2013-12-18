import re
import shutil
from tito.common import debug, run_command
from tito.builder import Builder, UpstreamBuilder

class RestraintBuilder(UpstreamBuilder):

    def tgz(self):
        if self.test:
            Builder.tgz(self)
        else:
            UpstreamBuilder.tgz(self)
            shutil.copy('%s/%s' % (self.rpmbuild_sourcedir, self.tgz_filename),
                    self.rpmbuild_basedir)
        run_command("pushd %s/third-party && make tarballs && popd" % self.rpmbuild_gitcopy)
        run_command("pushd %s && tar zcf %s %s && popd" % (self.rpmbuild_sourcedir,
                                                           self.tgz_filename, self._get_tgz_name_and_ver()))
	run_command("cp %s/%s %s/" %  \
                (self.rpmbuild_sourcedir, self.tgz_filename,
                    self.rpmbuild_basedir))
                                                           
