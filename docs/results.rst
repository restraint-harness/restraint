.. _task_results:

Task Results
------------
The final result outcome of a task is influenced by what is set when calling
`rstrnt-report-result`, `rstrnt-abort`, and the return code the task exits with.

The user controls the output of the task by calling `rstrnt-report-result` with
test results of `SKIP|PASS|WARN|FAIL` (listed by severity). It can be called multiple
times in the same task but the final task result wlll be the highest severity
reported so long as the task exits with zero. With these results, the job
will go on to the next task.

If the user also `rstrnt-abort`, this take precedence over the calls to
`rstrnt-report-results`. The final task result will be `abort` and the job
will not go on to the next task.

There is a deviation in behavior when a non-zero exit code is returned by the task.
If the legacy `Makefile/testinfo` file is present in the user's task, the final
task result is `ABORT` regardless of the restraint command calls the user makes.
If the `metadata` file is present in the user's task, the final task result is FAIL.
If the user still wants the legacy behavior, they can call the `rstrnt-abort` command
in their task.

For more details in regard to the command `rstrnt-report-result` and `rstrnt-abort`
refer to `restraintd` command section :ref:`restraintd_intro`.

