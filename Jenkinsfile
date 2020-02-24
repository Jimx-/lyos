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
                sh 'mkdir -p ./obj/destdir.x86'
                
                dir('toolchain') {
                    sh './download.sh'
                    sh 'BUILD_EVERYTHING=true ./setup.sh'
                }
                
                sh 'make SUBARCH=i686'
            }
        }
    }
}
