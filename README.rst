=========
Restraint
=========

Restraint is designed to execute tasks.  These tasks can be tests which
report results or simply code that you want to automate.
Which tasks to execute is determined by a job [#]_.  The job also describes
where to retrieve the tasks from and what parameters to pass in.  These 
tasks can report multiple PASS, FAIL, WARN results along with an optional
score.  Tasks also have the ability to report log files.  Each task can
have metadata describing dependencies and max run time for example.
Execution and reporting can be further enhanced with plugins.

Restraint can be used with Beaker [#]_ since it talks beaker's Harness API [#]_ for
reporting results.  But it can also be used stand alone.

Full Documentation is `here <http://restraint.readthedocs.org/en/latest/>`_.

.. [#] `Job XML <http://beaker-project.org/docs/user-guide/job-xml.html>`_.
.. [#] `Beaker <http://beaker-project.org>`_ is open-source software for managing and automating labs of test computers.
.. [#] `Alternate Harness <http://beaker-project.org/docs/alternative-harnesses>`_ API.

Matrix Community Chat
---------------------

`Matrix <https://matrix.org/>`_ community chat channel located at any of the below locations:

* `<https://chat.fedoraproject.org/#/room/#beaker:fedora.im>`_  Fedora Project's web client
* `<https://app.element.io/#/room/#beaker:fedora.im>`_  Element web client
* `<https://matrix.to/#/#beaker:fedora.im>`_
