/**
 * This is the cpp end of the JNI connection between the tool and the cpp vision
 * algorithm. See the method processFrame() implemented below.
 *
 * @author Johannes Strom
 * @author Mark McGranagan
 * @author Octavian Neamtu 
 *
 * @date November 2008
 */

#include <jni.h>

#include <iostream>
#include <vector>
#include <string>

#include <boost/shared_ptr.hpp>

#include "TOOL_Vision_TOOLVisionLink.h"
#include "Vision.h"
#include "NaoPose.h"
#include "Sensors.h"
#include "SensorDef.h"       // for NUM_SENSORS
#include "Kinematics.h"      // for NUM_JOINTS
//#include "CameraCalibrate.h"
#include "Structs.h"         // for estimate struct
#include "VisualFieldObject.h"
#include "VisualLine.h"
#include "Common.h"


using namespace std;
using namespace boost;

/**
 *
 * This is the central cpp method called by the Java TOOL to run vision
 * results. It takes in the raw image data, as well as the joint angles
 * and single byte array representation of the color table.
 * (possibly later it will also need to take other sensor values.)
 *
 * Due to the difficulty of JNI, we currently also require that the thresholded
 * array which Java wants filled be passed in as well. This removes the need
 * for us to construct a java byte[][] from cpp.
 *
 * This method explicitly returns nothing. The results of vision computation
 * are sent back to Java via some setter methods. Right now we only send back
 * the thresholded array after vision is done with it. In the future,
 * we should make a method to translate a cpp field object into a
 * Data/FieldObject.java compatible field object.
 *
 * KNOWN INEFFICIENCIES:In the future, we'd like to be able
 * to store a static file-level pointer to the vision class. A method which
 * instantiates vision  be called in the TOOLVisionLink.java  contructor.
 * This instance of vision will also need to be destroyed somehow when
 * we are done with this VisionLink
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

    //Instantiate the vision stuff
    static boost::shared_ptr<Sensors> sensors(new Sensors());
    static boost::shared_ptr<NaoPose> pose(new NaoPose(sensors));
    static boost::shared_ptr<Profiler> profiler(new Profiler(micro_time));
    static Vision vision(pose, profiler);

    JNIEXPORT void JNICALL Java_TOOL_Vision_TOOLVisionLink_cppProcessImage
    (JNIEnv * env, jobject jobj, jbyteArray jimg, jfloatArray jjoints,
     jfloatArray jsensors, jbyteArray jtable, jobjectArray thresh_target){
      
        //Size checking -- we expect the sizes of the arrays to match
        //Base these on the size cpp expects for the image
        unsigned int tlenw =
            env->GetArrayLength((jbyteArray)
                                env->GetObjectArrayElement(thresh_target,0));
        unsigned int numSensorsInFrame = env->GetArrayLength(jsensors);
        //If one of the dimensions is wrong, we exit
        if(env->GetArrayLength(jimg) != IMAGE_BYTE_SIZE) {
            cout << "Error: the image had the wrong byte size" << endl;
            cout << "Image byte size should be " << IMAGE_BYTE_SIZE << endl;
            cout << "Detected byte size of " << env->GetArrayLength(jimg)
                 << endl;
            return;
        }
        if (env->GetArrayLength(jjoints) != Kinematics::NUM_JOINTS) {
            cout << "Error: the joint array had incorrect dimensions" << endl;
            return;
        }
        // I (George) have disabled this check because we now have a variable
        // number of sensors depending on which version of the frame format
        // is loaded.
        /*
        if (numSensorsInFrame != NUM_SENSORS) {
            cout << "Warning: This frame must be old because the number of "
                    "sensors stored in it is\n"
                    "wrong. The missing values will be initialized to 0."
                 << endl;
        }
        */
        if (env->GetArrayLength(jtable) != YMAX*UMAX*VMAX) {
            cout << "Error: the color table had incorrect dimensions" << endl;
            return;
        }
        if (env->GetArrayLength(thresh_target) != IMAGE_HEIGHT ||
            tlenw != IMAGE_WIDTH) {
            cout << "Error: the thresh_target had incorrect dimensions" << endl;
            return;
        }

        //load the table
        jbyte *buf_table = env->GetByteArrayElements( jtable, 0);
        byte * table = (byte *)buf_table; //convert it to a reg. byte array
        vision.thresh->initTableFromBuffer(table);
        env->ReleaseByteArrayElements( jtable, buf_table, 0);

        // Set the joints data - Note: set visionBodyAngles not bodyAngles
        float * joints = env->GetFloatArrayElements(jjoints,0);
        vector<float> joints_vector(&joints[0],&joints[Kinematics::NUM_JOINTS]);
        env->ReleaseFloatArrayElements(jjoints,joints,0);
        sensors->setVisionBodyAngles(joints_vector);

        // Set the sensor data
        float * sensors_array = env->GetFloatArrayElements(jsensors,0);
        vector<float> sensors_vector(&sensors_array[0],
                                     &sensors_array[numSensorsInFrame]);
        env->ReleaseFloatArrayElements(jsensors,sensors_array,0);

        // If there are missing sensors values in the frame, the vector will be
        // shorter. We add 0s to it until it is of size NUM_SENSORS.
        for (unsigned int i = 0; i < NUM_SENSORS - numSensorsInFrame; ++i)
            sensors_vector.push_back(0.0f);

        sensors->setAllSensors(sensors_vector);

        // Clear the debug image on which the vision algorithm can draw
        vision.thresh->initDebugImage();

        //get pointer access to the java image array
        jbyte *buf_img = env->GetByteArrayElements( jimg, 0);
        byte * img = (byte *)buf_img; //convert it to a reg. byte array
        //timing the vision process
        long startTime = micro_time();
        //PROCESS VISION!!
        vision.notifyImage(img);
        long processTime = micro_time() - startTime;
        //vision.drawBoxes();
        env->ReleaseByteArrayElements( jimg, buf_img, 0);

        //copy results from vision thresholded to the array passed in from java
        //we access to each row in the java array, and copy in from cpp thresholded
        //we may in the future want to experiment with malloc, for increased speed
        for(int i = 0; i < IMAGE_HEIGHT; i++){
            jbyteArray row_target=
                (jbyteArray) env->GetObjectArrayElement(thresh_target,i);
            jbyte* row = env->GetByteArrayElements(row_target,0);

            for(int j = 0; j < IMAGE_WIDTH; j++){
                row[j]= vision.thresh->thresholded[i][j];
#ifdef OFFLINE
                if (vision.thresh->debugImage[i][j] != GREY) {
                    row[j]= vision.thresh->debugImage[i][j];
                }
#endif

            }
            env->ReleaseByteArrayElements(row_target, row, 0);
        }
        //get the id for the java class, so we can get method IDs
        jclass javaClass = env->GetObjectClass(jobj);

	//push the processTime
	jmethodID setProcessTime = env->GetMethodID(javaClass, "setProcessTime", "(I)V");
	env->CallVoidMethod(jobj, setProcessTime, (int) processTime);

        //push the ball
        jmethodID setBallInfo = env->GetMethodID(javaClass, "setBallInfo", "(DDIIIID)V");
        env->CallVoidMethod(jobj, setBallInfo,
                            vision.ball->getWidth(), vision.ball->getHeight(),
                            vision.ball->getX(), vision.ball->getY(),
                            vision.ball->getCenterX(), vision.ball->getCenterY(),
                            vision.ball->getRadius());

        //get the method ID for the field object setter
        jmethodID setFieldObjectInfo = env->GetMethodID(javaClass, "setFieldObjectInfo",
                                                        "(IDDIIIIIIII)V");

        //push each field object
        VisualFieldObject *obj;
        VisualCrossbar * cb;
		VisualCross *cross;
        int k = 0;
        while(k != -1) {
            //loop through all the objects we want to pass
            switch(k){
            case 0: obj = vision.bgrp; k++; cb = NULL; cross = NULL; break;
            case 1: obj = vision.bglp; k++; cb = NULL; cross = NULL; break;
            case 2: obj = vision.ygrp; k++; cb = NULL; cross = NULL; break;
            case 3: obj = vision.yglp; k++; cb = NULL; cross = NULL; break;
            case 4: cb = vision.ygCrossbar; k++; obj = NULL; cross = NULL; break;
            case 5: cb = vision.bgCrossbar; k++; obj = NULL; cross = NULL; break;
			case 6: cross = vision.cross; k++; cb = NULL; obj = NULL; break;
            default: k = -1; obj = NULL; cb = NULL; break;
            }
            if (obj != NULL) {
                int id = (int) obj->getID();
                if (obj->getPossibleFieldObjects()->size() > 1) {
                    if (id == BLUE_GOAL_LEFT_POST ||
                        id == BLUE_GOAL_RIGHT_POST ||
                        id == BLUE_GOAL_POST) {
                        id = BLUE_GOAL_POST;

                    } else {
                        id = YELLOW_GOAL_POST;
                    }
                }

                env->CallVoidMethod(jobj, setFieldObjectInfo,
                                    id,
                                    obj->getWidth(), obj->getHeight(),
                                    obj->getLeftTopX(), obj->getLeftTopY(),
                                    obj->getRightTopX(), obj->getRightTopY(),
                                    obj->getLeftBottomX(), obj->getLeftBottomY(),
                                    obj->getRightBottomX(), obj->getRightBottomY());
            } else if (cb != NULL) {
                env->CallVoidMethod(jobj, setFieldObjectInfo,
                                    47,
                                    cb->getWidth(), cb->getHeight(),
                                    cb->getLeftTopX(), cb->getLeftTopY(),
                                    cb->getRightTopX(), cb->getRightTopY(),
                                    cb->getLeftBottomX(), cb->getLeftBottomY(),
                                    cb->getRightBottomX(), cb->getRightBottomY());
            } else if (cross != NULL) {
                env->CallVoidMethod(jobj, setFieldObjectInfo,
                                    (int)cross->getID(),
                                    cross->getWidth(), cross->getHeight(),
                                    cross->getLeftTopX(), cross->getLeftTopY(),
                                    cross->getRightTopX(), cross->getRightTopY(),
                                    cross->getLeftBottomX(), cross->getLeftBottomY(),
                                    cross->getRightBottomX(), cross->getRightBottomY());
			}
        }

        //get the methodIDs for the visual line setter methods from java
        jmethodID setVisualLineInfo = env->GetMethodID(javaClass, "setVisualLineInfo",
                                                       "(IIII)V");
        jmethodID prepPointBuffers = env->GetMethodID(javaClass, "prepPointBuffers",
                                                      "(I)V");
        jmethodID setPointInfo = env->GetMethodID(javaClass, "setPointInfo",
                                                  "(IIDI)V");
        jmethodID setUnusedPointsInfo = env->GetMethodID(javaClass, "setUnusedPointsInfo",
                                                         "()V");
        jmethodID setVisualCornersInfo = env->GetMethodID(javaClass, "setVisualCornersInfo",
                                                          "(IIFFI)V");
        //push data from the lines object
        vector<VisualLine> expectedLines = pose->getExpectedVisualLinesFromFieldPosition(270, 180, 0);
            //368, 67, 1.57 + 3.14);
        const vector<VisualLine> *lines = &expectedLines;
            //vision.fieldLines->getLines();
        for (vector<VisualLine>::const_iterator i = lines->begin();
             i!= lines->end(); i++) {
            env->CallVoidMethod(jobj, prepPointBuffers,
                                i->points.size());
            for(vector<linePoint>::const_iterator j = i->points.begin();
                j != i->points.end(); j++) {
                env->CallVoidMethod(jobj, setPointInfo,
                                    j->x, j->y,
                                    j->lineWidth, j->foundWithScan);
            }
            env->CallVoidMethod(jobj, setVisualLineInfo,
                                i->start.x, i->start.y,
                                i->end.x, i->end.y);
        }
        //push data from unusedPoints
        const list <linePoint> *unusedPoints = vision.fieldLines->getUnusedPoints();
        env->CallVoidMethod(jobj, prepPointBuffers, unusedPoints->size());
        for (list <linePoint>::const_iterator i = unusedPoints->begin();
             i != unusedPoints->end(); i++)
            env->CallVoidMethod(jobj, setPointInfo,
                                i->x, i->y,
                                i->lineWidth, i->foundWithScan);
        env->CallVoidMethod(jobj, setUnusedPointsInfo);
        //push data from visualCorners
        const list <VisualCorner>* corners = vision.fieldLines->getCorners();
        for (list <VisualCorner>::const_iterator i = corners->begin();
             i != corners->end(); i++)
            env->CallVoidMethod(jobj, setVisualCornersInfo,
                                i->getX(), i->getY(),
								i->getDistance(), i->getBearing(),
								i->getShape());
        //horizon line
        jmethodID setHorizonInfo = env->GetMethodID(javaClass, "setHorizonInfo",
                                                    "(IIIII)V");
        env->CallVoidMethod(jobj, setHorizonInfo,
                            vision.pose->getLeftHorizon().x,
                            vision.pose->getLeftHorizon().y,
                            vision.pose->getRightHorizon().x,
                            vision.pose->getRightHorizon().y,
                            vision.thresh->getVisionHorizon());

        return;

    }

    JNIEXPORT void JNICALL Java_TOOL_Vision_TOOLVisionLink_cppPixEstimate
    (JNIEnv * env, jobject jobj, jint pixelX, jint pixelY,
     jfloat objectHeight, jdoubleArray estimateResult) {
        // make sure the array for the estimate is big enough. There
        // should be room for five things in there. (subject to change)
        if (env->GetArrayLength(estimateResult) !=
            sizeof(estimate)/sizeof(float)) {
            cout << "Error: the estimateResult array had incorrect "
                "dimensions" << endl;
            return;
        }

        //load the table
        jdouble * buf_estimate =
            env->GetDoubleArrayElements( estimateResult, 0);
        double * estimate_array =
            (double *)buf_estimate;
        //vision.thresh->initTableFromBuffer(table);
        estimate est = pose->pixEstimate(static_cast<int>(pixelX),
                                         static_cast<int>(pixelY),
                                         static_cast<float>(objectHeight));

        estimate_array[0] = est.dist;
        estimate_array[1] = est.elevation;
        estimate_array[2] = est.bearing;
        estimate_array[3] = est.x;
        estimate_array[4] = est.y;

        env->ReleaseDoubleArrayElements( estimateResult, buf_estimate, 0);
    }

JNIEXPORT void JNICALL Java_TOOL_Vision_TOOLVisionLink_cppGetCameraCalibrate
    (JNIEnv * env, jobject jobj, jfloatArray cameraCalibrate) {

         jfloat * cam_calibrate =
            env->GetFloatArrayElements( cameraCalibrate, 0);
         float * cam_calib = (float *) cam_calibrate;
         for (int i = 0; i < 9; i++)
            cam_calib[i] = CameraCalibrate::CAMERA_CALIBRATE[i];
         env->ReleaseFloatArrayElements( cameraCalibrate, cam_calibrate, 0);
    }

JNIEXPORT void JNICALL Java_TOOL_Vision_TOOLVisionLink_cppSetCameraCalibrate
    (JNIEnv * env, jobject jobj, jfloatArray cameraCalibrate) {

         jfloat * cam_calibrate =
            env->GetFloatArrayElements( cameraCalibrate, 0);
         float * cam_calib = (float *) cam_calibrate;
         CameraCalibrate::UpdateWithParams(cam_calib);
         env->ReleaseFloatArrayElements( cameraCalibrate, cam_calibrate, 0);
    }

#ifdef __cplusplus
}
#endif
