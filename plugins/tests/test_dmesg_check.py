import unittest
import subprocess
import os
import shutil
import threading
import labcontroller
import shutil

class TestDmesgCheck(unittest.TestCase):

    @classmethod
    def setUpClass(self):
        self.maxDiff = None

        self.fake_dmesg_path       = os.path.abspath('./bin')
        self.restraint_bins_path   = os.path.abspath('../../src')
        self.server_output_path    = os.path.abspath('./server_output')
        self.restraint_plugins_dir = os.path.abspath('../report_result.d')

        if not os.path.exists(self.server_output_path):
            os.makedirs(self.server_output_path)

        self.env = {
            'PATH':':'.join([self.fake_dmesg_path, self.restraint_bins_path, os.environ['PATH']]),
            'RSTRNT_RESULT_URL' :'http://localhost:8001/recipes/1/tasks/1/results/1',
            'RSTRNT_PLUGINS_DIR': self.restraint_plugins_dir,
            'RSTRNT_DISABLED'   : "10_avc_check 20_avc_clear",
            'RECIPE_URL'        : "http://localhost:8001/recipes/1",
            'TASKID'            : "1"
        }

        if 'FALSESTRINGS' in os.environ:
            del os.environ['FALSESTRINGS']
        if 'FAILURESTRINGS' in os.environ:
            del os.environ['FAILURESTRINGS']

        t = threading.Thread(target=labcontroller.app.run, name='HTTP server', kwargs={'port': 8001})
        t.daemon = True
        t.start()

    def setUp(self):
        shutil.rmtree(self.server_output_path)
        os.makedirs(self.server_output_path)

        self.assertEqual(os.listdir(self.server_output_path), [], 'Server output was not emptied!')

        output = subprocess.call(['./../run_plugins'], env=self.env)

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
Initializing cgroup subsys cpuset
Initializing cgroup subsys cpu
NMI appears to be stuck
Blah blah
Badness at
blah bloop
NMI appears to be stuck
blip blop
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
