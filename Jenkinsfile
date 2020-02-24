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
                sh 'mkdir -p ./obj/destdir.x86/bin'
                
                dir('toolchain') {
                    sh './download.sh'
                    sh 'BUILD_EVERYTHING=true ./setup.sh'
                }
                
                sh 'make SUBARCH=i686'
            }
        }
    }
}
