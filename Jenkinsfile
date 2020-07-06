pipeline {
    agent none
    
    stages {
        stage('Build') {
            parallel {
                stage('i686') {
                    agent {
                        dockerfile {
                            filename 'Dockerfile'
                            args '--privileged'
                        }
                    }

                    stages {
                        stage('Build - i686') {
                            steps {
                                sh 'make SUBARCH=i686 defconfig'
                                sh 'make SUBARCH=i686 objdirs'

                                dir('toolchain') {
                                    sh './download.sh'
                                    sh 'BUILD_EVERYTHING=true ./setup.sh -m i686'
                                }

                                ansiColor('xterm') {    
                                    sh 'make SUBARCH=i686'
                                }
                            }                        
                        }

                        stage('Make image - i686') {
                            steps {
                                sh './scripts/setup-disk.sh'
                                sh 'mv lyos-disk.img lyos-i686.img'
                                sh 'xz --fast lyos-i686.img'
                            }                        
                        }

                        stage('Collect - i686') {
                            steps {
                                archiveArtifacts 'lyos-i686.img.xz'
                            }                        
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
