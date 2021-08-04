pipeline {
    agent none

    stages {
        stage('Build') {
            parallel {
                stage('Build - i686') {
                    agent {
                        dockerfile true
                    }

                    steps {
                        sh 'make SUBARCH=i686 defconfig'
                        sh 'make SUBARCH=i686 objdirs'
                        sh 'make SUBARCH=i686 install-headers'

                        dir('toolchain') {
                            sh './download.sh'
                            sh 'BUILD_EVERYTHING=true ./setup.sh -m i686'
                        }

                        ansiColor('xterm') {
                            sh 'make SUBARCH=i686'
                        }

                        dir('toolchain') {
                            sh 'BUILD_EVERYTHING=true ./setup-extra.sh -m i686'                        
                            sh 'BUILD_EVERYTHING=true ./setup-x11.sh -m i686'
                        }
                    }

                    post {
                        cleanup {
                            deleteDir()
                        }
                    }
                }

                stage('Build - RISC-V') {
                    agent {
                        dockerfile true
                    }

                    steps {
                        sh 'make SUBARCH=riscv64 defconfig'
                        sh 'make SUBARCH=riscv64 objdirs'
                        sh 'make SUBARCH=riscv64 install-headers'

                        dir('toolchain') {
                            sh './download.sh'
                            sh 'BUILD_EVERYTHING=true ./setup.sh -m riscv64'
                        }

                        ansiColor('xterm') {
                            sh 'make SUBARCH=riscv64'
                        }                        
                    }

                    post {
                        cleanup {
                            deleteDir()
                        }
                    }
                }
            }
        }
    }
}