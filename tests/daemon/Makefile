SHELL = /bin/bash

SSH_KEY := testkey

.PHONY: check-keys-setup
check-keys-setup:
	@mkdir -p ../../keys; \
	pushd ../../keys; \
	if ! test -f $(SSH_KEY).pub; then \
		ssh-keygen -f $(SSH_KEY) -t rsa -q -N ""; \
	fi; \
	popd

.PHONY: check-daemon-setup-testmachine
check-daemon-setup-testmachine:
	@echo "SETTING UP TESTMACHINE in restraint-testmachine git-worktree"
	@echo "============================================================"
	@git worktree add -d ../../../restraint-testmachine 2>/dev/null || \
		echo "Worktree already exists, not copying changes over"; \
	pushd ../../../restraint-testmachine > /dev/null ; \
	if ! test -f ./keys/$(SSH_KEY).pub; then \
		mkdir -p keys; \
		cp ../restraint/keys/$(SSH_KEY).pub ./keys/; \
	fi; \
	if ! podman container exists "restraint-checks-tm-fedora-latest"; then \
		./containers/run.sh -e /restraint/containers/testmachine.sh -c tm make install; \
	fi; \
	./containers/run.sh -c tm /usr/sbin/sshd -D &
	# let container start to avoid race with testclient
	sleep 1


.PHONY: check-daemon-setup-testclient
check-daemon-setup-testclient:
	@pushd ../../ > /dev/null ; \
	if ! podman container exists "restraint-checks-tc-fedora-latest"; then \
		./containers/run.sh -e /restraint/containers/testclient.sh -c tc make install; \
	fi; \
	./containers/run.sh -c tc make -C tests/daemon check-daemon; \
	popd

.PHONY: check-daemon-container
check-daemon-container: check-keys-setup check-daemon-setup-testmachine check-daemon-setup-testclient stop-container

.PHONY: check-daemon
check-daemon:
	# private container network, should be no MITM vulnerabilities
	ssh -o StrictHostKeyChecking=no restraint-checks-tm-fedora-latest echo "HELLO from TESTMACHINE!!!"
	restraint -t restraint-checks-tm-fedora-latest -j test.xml

.PHONY: stop-container
stop-container:
	# stop the testmachine by killing the sshd
	@podman exec -ti restraint-checks-tm-fedora-latest killall sshd
	@echo "Waiting for testmachine to stop"

.PHONY: clean-container
clean-container:
	podman rm restraint-checks-tc-fedora-latest
	podman rm restraint-checks-tm-fedora-latest
