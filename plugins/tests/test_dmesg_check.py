import unittest
import subprocess
import os
import shutil
import threading
import labcontroller
import shutil

TEST_FAIL_FILE = os.path.abspath('./failurestrings')
TEST_FALSE_FILE = os.path.abspath('./falsestrings')

class DmesgCheckBase(unittest.TestCase):

    def _setUpClass(self, port):
        self.maxDiff = None

        self.restraint_bins_path = os.path.abspath('../../src')
        self.server_output_path = os.path.abspath('./server_output')
        self.restraint_plugins_dir = os.path.abspath('../report_result.d')

        if not os.path.exists(self.server_output_path):
            os.makedirs(self.server_output_path)

        self.env = {
            'FAILUREFILENM'     : TEST_FAIL_FILE,
            'FALSEFILENM'       : TEST_FALSE_FILE,
            'PATH':':'.join([self.fake_dmesg_path, self.restraint_bins_path, os.environ['PATH']]),
            'RSTRNT_RESULT_URL' :'http://localhost:%s/recipes/1/tasks/1/results/1' % port,
            'RSTRNT_PLUGINS_DIR': self.restraint_plugins_dir,
            'RSTRNT_DISABLED'   : "10_avc_check 20_avc_clear",
            'RECIPE_URL'        : "http://localhost:%s/recipes/1" % port,
            'TASKID'            : "1"
        }

        self.t = threading.Thread(target=labcontroller.app.run, name='HTTP server', kwargs={'port': port})
        self.t.daemon = True
        self.t.start()

    def _setUpStrings(self):

        if 'FALSESTRINGS' in os.environ:
            del os.environ['FALSESTRINGS']
        if 'FAILURESTRINGS' in os.environ:
            del os.environ['FAILURESTRINGS']

    def setUp(self):
        shutil.rmtree(self.server_output_path)
        os.makedirs(self.server_output_path)

        self.assertEqual(os.listdir(self.server_output_path), [], 'Server output was not emptied!')

        self._setUpStrings()

        output = subprocess.call(['./../run_plugins'], env=self.env)


class TestDmesgCheckHardcodeDflt(DmesgCheckBase):

    @classmethod
    def setUpClass(self):
        self.fake_dmesg_path = os.path.abspath('./bin')

        self._setUpClass(self, 8002)

    def test_dmesg_check_for_correct_output(self):
        expected = """------------[ cut here ]------------
WARNING: at kernel/rh_taint.c:13 mark_hardware_unsupported+0x39/0x40() (Not tainted)
Call Trace:
[<ffffffff8106e2e7>] ? warn_slowpath_common+0x87/0xc0
[<ffffffff8106e37f>] ? warn_slowpath_fmt_taint+0x3f/0x50
[<ffffffff8109f399>] ? mark_hardware_unsupported+0x39/0x40,[<ffffffff81c2d9e2>] ? setup_arch+0xb57/0xb7a
[<ffffffff8150d0d1>] ? printk+0x41/0x48\n[<ffffffff81c27c33>] ? start_kernel+0xdc/0x430
[<ffffffff81c2733a>] ? x86_64_start_reservations+0x125/0x129
[<ffffffff81c27438>] ? x86_64_start_kernel+0xfa/0x109
---[ end trace a7919e7f17c0a725 ]---
NMI appears to be stuck
Badness at
NMI appears to be stuck
====================================================
DMESG Selectors:
Used Default FAILURESTRINGS and Default FALSESTRINGS
====================================================
FAILURESTRINGS: \bOops\b|\bBUG\b|NMI appears to be stuck|Badness at
FailureStrings file not found.
====================================================
FALSESTRINGS: BIOS BUG|DEBUG|mapping multiple BARs.*IBM System X3250 M4
FalseStrings file not found.
====================================================
"""
        with open(self.server_output_path + '/resultoutputfile.log', 'r') as f:
            outputfile_text = f.read()
        self.assertMultiLineEqual(outputfile_text, expected)

    def test_rstrnt_report_log_sends_dmesg_log(self):
        with open(self.fake_dmesg_path + '/dmesg.log') as f1:
            with open(self.server_output_path + '/dmesg.log') as f2:
                self.assertMultiLineEqual(f1.read(), f2.read())


class TestDmesgCheckFileDflt(DmesgCheckBase):

    @classmethod
    def setUpClass(self):
        self.fake_dmesg_path = os.path.abspath('./bin2')
        self._setUpClass(self, 8003)

    def _setUpStrings(self):

        with open(TEST_FAIL_FILE, "w") as f_file:
            f_file.write("Something is stuck\n")
            f_file.write("    \n")
            f_file.write("Coolness at\n")
            f_file.write("\n")

        with open(TEST_FALSE_FILE, "w") as f_file:
            f_file.write("DEBUG\n")
            f_file.write("    \n")
            f_file.write("mark_hardware_unsupported")
            f_file.write("\n")

    def tearDown(self):
        os.remove(TEST_FAIL_FILE);
        os.remove(TEST_FALSE_FILE);

    def test_dmesg_check_for_correct_output(self):
        expected = """------------[ cut here ]------------
WARNING: at arch/x86/mm/ioremap.c:195 __ioremap_caller+0x286/0x370()
Info: mapping multiple BARs. Your kernel is fine.
Hardware name: IBM IBM System X3250 M4 -[2583AC1]-/00D3729, BIOS -[JQE164AUS-1.07]- 12/09/2013
 ffff88023241ba88 00000000653d7f8a ffff88023241ba40 ffffffff816350c1
 ffff88023241ba78 ffffffff8107b200 ffffc90004e50000 ffffc90004e50000
 00000000fed18000 ffffc90004e50000 0000000000008000 ffff88023241bae0
Call Trace:
 [<ffffffff816350c1>] dump_stack+0x19/0x1b
 [<ffffffff8107b200>] warn_slowpath_common+0x70/0xb0
 [<ffffffff810ed4ae>] load_module+0x134e/0x1b50
 [<ffffffff813166b0>] ? ddebug_proc_write+0xf0/0xf0
 [<ffffffff810e9743>] ? copy_module_from_fd.isra.42+0x53/0x150
 [<ffffffff810ede66>] SyS_finit_module+0xa6/0xd0
 [<ffffffff816457c9>] system_call_fastpath+0x16/0x1b
---[ end trace 5fcf161d6e45465f ]---
Something is stuck
Coolness at
Something is stuck
====================================================
DMESG Selectors:
Used failurestrings file and falsestrings file
====================================================
FAILURESTRINGS: Something is stuck|Coolness at
FailureStrings file found and contains:
Something is stuck
    
Coolness at

====================================================
FALSESTRINGS: DEBUG|mark_hardware_unsupported
FalseStrings file found and contains:
DEBUG
    
mark_hardware_unsupported
====================================================
"""
        with open(self.server_output_path + '/resultoutputfile.log', 'r') as f:
            outputfile_text = f.read()
        self.assertMultiLineEqual(outputfile_text, expected)

    def test_rstrnt_report_log_sends_dmesg_log(self):
        with open(self.fake_dmesg_path + '/dmesg.log') as f1:
            with open(self.server_output_path + '/dmesg.log') as f2:
                self.assertMultiLineEqual(f1.read(), f2.read())


class TestDmesgCheckVariableSetting(DmesgCheckBase):

    @classmethod
    def setUpClass(self):
        self.fake_dmesg_path = os.path.abspath('./bin3')
        self.port = 8084
        self._setUpClass(self, self.port)

    def _setUpStrings(self):

        self.env = {
            'FAILUREFILENM'     : TEST_FAIL_FILE,
            'FALSEFILENM'       : TEST_FALSE_FILE,
            'FAILURESTRINGS'    : "My Head Hurts|My feet hurt",
            'FALSESTRINGS'      : "get aspirin|My System X3250 M4",
            'PATH':':'.join([self.fake_dmesg_path, self.restraint_bins_path, os.environ['PATH']]),
            'RSTRNT_RESULT_URL' :'http://localhost:%s/recipes/1/tasks/1/results/1' % self.port,
            'RSTRNT_PLUGINS_DIR': self.restraint_plugins_dir,
            'RSTRNT_DISABLED'   : "10_avc_check 20_avc_clear",
            'RECIPE_URL'        : "http://localhost:%s/recipes/1" % self.port,
            'TASKID'            : "1"
        }

        ## Let's make sure variables take precedence over files
        ## and hardcoded defaults.
        with open(TEST_FAIL_FILE, "w") as f_file:
            f_file.write("Something is stuck\n")
            f_file.write("    \n")
            f_file.write("Coolness at\n")
            f_file.write("\n")

        with open(TEST_FALSE_FILE, "w") as f_file:
            f_file.write("DEBUG\n")
            f_file.write("    \n")
            f_file.write("mark_hardware_unsupported")
            f_file.write("\n")

    def tearDown(self):
        os.remove(TEST_FAIL_FILE);
        os.remove(TEST_FALSE_FILE);

    def test_dmesg_check_for_correct_output(self):
        expected = """My Head Hurts
My feet hurt
My Head Hurts
====================================================
DMESG Selectors:
Used FAILURESTRINGS Environment Variable and FALSESTRINGS Environment Variable
====================================================
FAILURESTRINGS: My Head Hurts|My feet hurt
FailureStrings file found and contains:
Something is stuck
    
Coolness at

====================================================
FALSESTRINGS: get aspirin|My System X3250 M4
FalseStrings file found and contains:
DEBUG
    
mark_hardware_unsupported
====================================================
"""
        with open(self.server_output_path + '/resultoutputfile.log', 'r') as f:
            outputfile_text = f.read()
        self.assertMultiLineEqual(outputfile_text, expected)

    def test_rstrnt_report_log_sends_dmesg_log(self):
        with open(self.fake_dmesg_path + '/dmesg.log') as f1:
            with open(self.server_output_path + '/dmesg.log') as f2:
                self.assertMultiLineEqual(f1.read(), f2.read())

if __name__ == '__main__':
    unittest.main()
