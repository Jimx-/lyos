pipeline {
    agent none
    
    stages {
        stage('Build') {
            parallel {
                stage('i686') {
                    agent {
                        dockerfile true
                    }

                    steps {
                        sh 'make SUBARCH=i686 defconfig'
                        sh 'make SUBARCH=i686 objdirs'

                        dir('toolchain') {
                            sh './download.sh'
                            sh 'BUILD_EVERYTHING=true ./setup.sh'
                        }

                        ansiColor('xterm') {    
                            sh 'make SUBARCH=i686'
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
