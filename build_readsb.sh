#!/bin/bash
ORIGIN_PATH=$PWD
THIS_SCRIPT_PATH=$(dirname $(readlink -f $0))
source $THIS_SCRIPT_PATH/build_helper

if [ "$UID" = 0 ]
then
  SUDO=''
else
  SUDO='sudo -S -E'
fi

set_env_vars

mkdir -p $ROOT_DIR/shared/protobuf
mkdir -p $ROOT_DIR/shared/protobuf_c
mkdir -p $ROOT_DIR/logs

proto_path=$ROOT_DIR/shared/protobuf
proto_c_path=$ROOT_DIR/shared/protobuf_c

echo !!!!!!installing protobuf!...
install_protobuf_from_source
echo !!!!!!installing protobuf C!...
install_protobuf_c_from_source