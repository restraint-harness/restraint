========================
Restraint Daemon Testing
========================

This area is designed to allow using containers to test the restraint client
on containerA to run tests on the restraint daemon on containerB using a
private container network bridge.

The goal is to make duplicating a user's workflow easy by just utilizing an
xml they have provided or using the default inside this directory.  Having a
collection of user workflows will make it easier to make changes to
restraint and test for regressions across a variety of workflows safely.  It
should also make it easier to duplicate and debug new workflow issues
locally.

The main logic is


  ContainerA                     ContainerB
  TestMachine                    TestClient

  sshd           <-------        ssh / restraint
  restraintd       ----->        git repo



Underneath the hood

To avoid sharing code, create a git worktree to the TestMachine that copies
over the restraint code to be built/installed in the container.

Create ssh keys and share them between the containers to automate the
communication.

Run the requested job xml on the test machine.
