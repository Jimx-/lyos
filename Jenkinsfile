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
                sh 'make silentoldconfig'
                
                dir('toolchain') {
                    sh './download.sh'
                    sh 'BUILD_EVERYTHING=true ./setup.sh'
                }
                
                sh 'make SUBARCH=i686'
            }
        }
    }
}
