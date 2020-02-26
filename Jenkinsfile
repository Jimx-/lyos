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
                
                ansiColor('xterm') {    
                    sh 'make SUBARCH=i686'
                }
            }
        }
    }
}
