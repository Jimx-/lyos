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
                sh '''
                    cd toolchain
                    ./download.sh
                    BUILD_EVERYTHING=true ./setup.sh
                    cd ..
                	make SUBARCH=i686
                '''
            }
        }
    }
}
