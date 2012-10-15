inherit srctree gitver kernel siteinfo

SECTION = "kernel"
DESCRIPTION = "Hanover Linux Kernel"
LICENSE = "GPLv2"
KERNEL_IMAGETYPE = "uImage"

PV = "${GITVER}"

FILE_DIRNAME = "${OEBASE}/hanover/recipes/linux-omap3"

COMPATIBLE_MACHINE = "dm814x-z3"

#CMDLINE = "console=tty1 root=/dev/mmcblk0p1 rootdelay=8 fbcon=rotate:1 panic=30 mem=110M"


MULTI_CONFIG_BASE_SUFFIX = ""

KERNEL_IMAGE_BASE_NAME = "${KERNEL_IMAGETYPE}-${PV}-${MACHINE}"
MODULES_IMAGE_BASE_NAME = "modules-${PV}-${MACHINE}"

do_configure() {
       #make ti8148_evm_defconfig
       make z3_814x_mod_defconfig    
}


do_deploy() {
        install -d ${DEPLOY_DIR_IMAGE}
        install -m 0644 ${KERNEL_OUTPUT} ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGE_BASE_NAME}.bin
        #package_stagefile_shell ${S}/${KERNEL_OUTPUT}
        #package_stagefile_shell ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGE_BASE_NAME}.bin

        if [ -d "${PKGD}/lib" ]; then
                #fakeroot tar -cvzf ${DEPLOY_DIR_IMAGE}/${MODULES_IMAGE_BASE_NAME}.tgz -C ${PKGD} lib
                fakeroot tar -cvzf ${DEPLOY_DIR_IMAGE}/${MODULES_IMAGE_BASE_NAME}.tgz lib
                #package_stagefile_shell ${DEPLOY_DIR_IMAGE}/${MODULES_IMAGE_BASE_NAME}.tgz
        fi

        cd ${DEPLOY_DIR_IMAGE}
        rm -f ${KERNEL_IMAGE_SYMLINK_NAME}.bin
        ln -sf ${KERNEL_IMAGE_BASE_NAME}.bin ${KERNEL_IMAGE_SYMLINK_NAME}.bin
        #package_stagefile_shell ${DEPLOY_DIR_IMAGE}/${KERNEL_IMAGE_SYMLINK_NAME}.bin
}