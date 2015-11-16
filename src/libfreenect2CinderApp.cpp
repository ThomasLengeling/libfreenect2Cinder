/*
 Kinect v2 example usage for Cinder library
 Thomas Sanchez Lengeling
*/

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"


//libfreenect2

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/logger.h>

using namespace ci;
using namespace ci::app;
using namespace std;

class libfreenect2CinderApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
    void cleanup() override;
    
    libfreenect2::Freenect2 freenect2;
    libfreenect2::Freenect2Device *dev = 0;
    libfreenect2::PacketPipeline *pipeline = 0;
    libfreenect2::SyncMultiFrameListener * listener;
    
    int mode;
    bool startKinect;
    
    //libfreenect2::Frame undistorted;
    //libfreenect2::Frame registered;
    libfreenect2::Registration* registration;
    
    libfreenect2::FrameMap frames;
    
    gl::Texture2dRef   mDepth;
    gl::Texture2dRef   mColor;
    gl::Texture2dRef   mIR;
    gl::GlslProgRef    normalizeShader;
    
};

bool protonect_shutdown = false; ///< Whether the running application should shut down.

void sigint_handler(int s)
{
    protonect_shutdown = true;
}


void libfreenect2CinderApp::setup()
{
    mode = 3;
    startKinect = true;
    
    if(freenect2.enumerateDevices() == 0)
    {
        std::cout << "no device connected!" << std::endl;
        mode = 0;
        startKinect = false;
    }
    
    std::string serial = "123456789";
    
    if(startKinect)
    {
        serial = freenect2.getDefaultDeviceSerialNumber();
        
        if(mode == 1) //cpu
        {
            if(!pipeline)
                pipeline = new libfreenect2::CpuPacketPipeline();
            
            if(pipeline == NULL){
                std::cerr << "CPU pipeline is not supported!" << std::endl;
                startKinect = false;
            }
        }
        else if(mode == 2)
        {
            if(!pipeline)
                pipeline = new libfreenect2::OpenGLPacketPipeline();
            
            if(pipeline == NULL){
                std::cerr << "GPU pipeline is not supported!" << std::endl;
                startKinect = false;
            }
        }
        else if(mode == 3)
        {
            if(!pipeline)
                pipeline = new libfreenect2::OpenCLPacketPipeline();
            
            if(pipeline == NULL){
                std::cerr << "CL pipeline is not supported!" << std::endl;
                startKinect = false;
            }
        }
    }
    
    if(startKinect){
        if(pipeline)
        {
            dev = freenect2.openDevice(serial, pipeline);
        }
        else
        {
            dev = freenect2.openDevice(serial);
        }
        
        if(dev == 0)
        {
            startKinect = false;
            std::cerr << "failure opening device!" << std::endl;
        }
    }
    
    if(startKinect){
        listener = new  libfreenect2::SyncMultiFrameListener(libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
        libfreenect2::FrameMap frames;
        
        dev->setColorFrameListener(listener);
        dev->setIrAndDepthFrameListener(listener);
        dev->start();
        
        std::cout << "device serial: " << dev->getSerialNumber() << std::endl;
        std::cout << "device firmware: " << dev->getFirmwareVersion() << std::endl;
        
        registration = new libfreenect2::Registration(dev->getIrCameraParams(), dev->getColorCameraParams());
        
        
        mDepth = gl::Texture2d::create(512, 424, gl::Texture::Format().swizzleMask( GL_RED, GL_RED, GL_RED, GL_ONE ).loadTopDown(true));
        mColor = gl::Texture2d::create(1920, 1080);
        mIR    = gl::Texture2d::create(512, 424 , gl::Texture::Format().swizzleMask( GL_RED, GL_RED, GL_RED, GL_ONE ).loadTopDown(true));
    
        mColor->setTopDown(true);
    }
    
    setWindowSize(ci::ivec2(512*2, 424*2));
    
    
    try{
        normalizeShader = gl::GlslProg::create(
                                               
                                               // vertex code
                                               CI_GLSL( 150,
                                                       
                                                       uniform mat4        ciModelViewProjection;
                                                       in vec4             ciPosition;
                                                       in vec4             ciTexCoord0;
                                                       
                                                       out  vec2           TexCoord0;
                                                       
                                                       void main( void )
                                                       {
                                                           TexCoord0          = ciTexCoord0.st;
                                                           gl_Position        = ciModelViewProjection * ciPosition;
                                                       }
                                                       
                                                       ),
                                               
                                               // fragment code
                                               
                                               CI_GLSL( 150,
                                                       out vec4             oColor;
                                                       
                                                       uniform sampler2D    uTex0;
                                                       in vec2              TexCoord0;
                                                       
                                                       void main( void ){
                                                           vec4 color = texture(uTex0, TexCoord0);
                                                           oColor = color;
                                                       }
                                                       
                                                       ) );
    }catch(std::exception &exc ) {
        console()<<"exception caught, type: " << exc.what()<<std::endl;
    }

    
}

void libfreenect2CinderApp::mouseDown( MouseEvent event )
{
}

void libfreenect2CinderApp::update()
{
}

void libfreenect2CinderApp::draw()
{
    gl::clear( Color( 0, 0, 0 ) );
    
    if(startKinect){
        listener->waitForNewFrame(frames);
        libfreenect2::Frame *rgb  = frames[libfreenect2::Frame::Color];
        libfreenect2::Frame *ir   = frames[libfreenect2::Frame::Ir];
        libfreenect2::Frame *depth = frames[libfreenect2::Frame::Depth];
        
        float * newDepth = (float *)malloc(512 * 424 * sizeof(float));
        float * newDepthFs = new float [512 * 424];
        memcpy(newDepth, reinterpret_cast<const float * >(depth->data), 512 * 424 * 4);
        
        float * newIR =  (float *)malloc(512 * 424 * sizeof(float));
        memcpy(newIR, reinterpret_cast<const float * >(ir->data), 512 * 424 * 4);
        
        
        //int count = 0;
        //float * pointerCounter = newDepth;
        float  endPointer = 512*424;
        int  i = 0;
        while(i < endPointer){
            newDepth[i] /= 4500.0f;
            newIR[i] /= 4500.0f;
            ++i;
        }
       
    
        //GL_r8
        //GL_RED
        //GL_RGB32F
        mIR->update(newIR, GL_RED, GL_FLOAT, 0, 512, 424);
        mDepth->update(newDepth, GL_RED, GL_FLOAT, 0, 512, 424);
        mColor->update(rgb->data, GL_BGRA, GL_UNSIGNED_BYTE, 0, 1920, 1080);
        
        listener->release(frames);
    
    }
    
        mDepth->setTopDown();
    
    {
        gl::ScopedMatrices mat;
        gl::ScopedGlslProg spdShader(normalizeShader);
        gl::ScopedTextureBind scdDepth(mDepth);
        //gl::ScopedColor col(ci::ColorA(1, 1, 1, 1));
    
        gl::drawSolidRect(mDepth->getBounds());
    }
    
    {
        gl::ScopedMatrices mat;
        // gl::ScopedColor col(ci::ColorA(1, 1, 1, 1));
        gl::translate(512, 0, 0);
        gl::ScopedGlslProg spdShader(normalizeShader);
        gl::ScopedTextureBind scdDepth(mIR);
        
       
       gl::drawSolidRect(ci::Rectf(0, 0, 512, 424));
    }
    
    
    {
        gl::ScopedMatrices mat;
        gl::translate(0, 424, 0);
        gl::draw(mColor, Rectf(0, 0, 512, 424));
    }
    
}

void libfreenect2CinderApp::cleanup()
{
    if(startKinect){
        dev->stop();
        dev->close();
        delete listener;
        delete registration;
    }
}

CINDER_APP( libfreenect2CinderApp, RendererGl )
