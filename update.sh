LOCAL_SERVER="192.168.1.1:/www/wbm"
REMOTE_SERVER="fw.qmp.cat:/var/www/firmware/wbm6"
CONFIGS="configs"
ENVS=$(for c in $CONFIGS/*; do echo ${c#*/}; done)

[ -z "$ENVS" ] && exit 1

if [ "$1" == "compile" ]; then
	for e in $ENVS; do 
		sh set_environment.sh $e
		make package/wbm-testbed/clean
		make -j4
	done
fi

if [ "$1" == "fastcompile" ]; then
	for e in $ENVS; do 
		sh set_environment.sh $e
		make package/wbm-testbed/{clean,install}
		make package/install
		make target/linux/install 
	done
fi

if [ "$1" == "repository" ]; then
	for e in $ENVS; do
		echo "Preparing repository for $e"
		sh set_environment.sh $e
		make -j4
	done
fi

if [ "$2" == "upload" ]; then

	echo "UPLOADING FIRMWARE"

	if [ ! -z "$LOCAL_SERVER" ]; then
		scp bin/ar71xx/openwrt-ar71xx-generic-tl-*sysupgrade*.bin $LOCAL_SERVER
		scp bin/x86/openwrt-x86-alix2-combined-ext4.img.gz $LOCAL_SERVER
	fi
	
	if [ ! -z "$REMOTE_SERVER" ]; then
		scp bin/ar71xx/openwrt-ar71xx-generic-tl-*sysupgrade*.bin $REMOTE_SERVER
		scp bin/x86/openwrt-x86-alix2-combined-ext4.img.gz $REMOTE_SERVER
	fi
fi	


