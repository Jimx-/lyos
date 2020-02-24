pipeline {
    agent {
        dockerfile true
    }
    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }
        stage('Compile') {
            steps {
                sh 'make SUBARCH=i686 defconfig'
                sh 'make SUBARCH=i686 objdirs'
                
                dir('toolchain') {
                    sh './download.sh'
                    sh 'BUILD_EVERYTHING=true ./setup.sh'
                }
                
                sh 'make SUBARCH=i686 genconf'
                sh 'make SUBARCH=i686 libraries'
                sh 'make SUBARCH=i686 fs'
                sh 'make SUBARCH=i686 drivers'
                sh 'make SUBARCH=i686 servers'
                sh 'make SUBARCH=i686 kernel'
            }
        }
    }
}
