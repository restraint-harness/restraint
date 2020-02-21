#!/bin/bash
#
# script: run.sh

readonly CONTAINER_NAME_BASE=restraint-checks
readonly IMAGE='fedora:latest'
readonly MOUNT_POINT=/restraint
readonly BOOTSTRAP=${MOUNT_POINT}/containers/bootstrap.sh
readonly PROVISION_DIR=${MOUNT_POINT}/containers/provision

main()
{
	local cmd
	local container_name
	local entry_point
	local image
	local provision
	local provision_script
	local work_dir

	provision=false

	while getopts "i:e:p:w:" opt ; do
		case ${opt} in
			i)
				image=${OPTARG}
				;;
			e)
				entry_point=${OPTARG}
				provision=true
				;;
			p)
				provision_script=${OPTARG}
				provision=true
				;;
			w)
				work_dir=$(realpath "${OPTARG}")
				;;
			*)
				:
		  esac
	done

	shift $(( OPTIND - 1 ))

	if [ -z "${image}" ] ; then
		image=${IMAGE}
	fi

	if [ -z "${provision_script}" ] ; then
		provision_script=${PROVISION_DIR}/${image}.sh
	fi

	container_name="${CONTAINER_NAME_BASE}-${image//:/-}"

	if ! podman container exists "${container_name}" ; then
		podman create --tty --interactive \
			      --volume "${work_dir:-$PWD}":"${MOUNT_POINT}":rw,Z \
			      --workdir "${MOUNT_POINT}" \
			      --name "${container_name}" \
			      "${image}"
		provision=true
	fi

	if "${provision}" ; then
		cmd="${entry_point:-${BOOTSTRAP}} -r ${provision_script} $*"
	else
		cmd=$*
	fi

	podman start "${container_name}"
	podman exec --tty --interactive "${container_name}" ${cmd:-/bin/bash}
	podman stop "${container_name}" || :
}

main "$@"
