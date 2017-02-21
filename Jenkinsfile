#!groovy

pipeline {

  agent {
    docker {
      image "bernddoser/docker-devel-cpp:ubuntu-gcc-4.9"
      label "docker-nodes"
    }
  }
  
  stages {

    stage("Build") {
      steps {
        sh 'mkdir -p build && cd build'
        sh 'cd build && cmake -DCMAKE_BUILD_TYPE=release -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DGMX_BUILD_MDRUN_ONLY=OFF -DGMX_BUILD_FDA=ON -DGMX_DEFAULT_SUFFIX=OFF -DGMX_BINARY_SUFFIX=_fda -DGMX_SIMD=NONE -DGMX_BUILD_UNITTESTS=ON -DGMX_BUILD_OWN_FFTW=ON -DGMX_GPU=OFF ..'
        sh 'cd build && make'
      }
    }
    
    stage("Test") {
      steps {
        sh 'cd build && make check'
      }
    }
  }

//  post {
//    always {
//      deleteDir()
//    }
//    failure {
//      mail to:"bernd.doser@h-its.org", subject:"FAILURE: ${currentBuild.fullDisplayName}", body: "Boo, we failed."
//    }
//  }

}
