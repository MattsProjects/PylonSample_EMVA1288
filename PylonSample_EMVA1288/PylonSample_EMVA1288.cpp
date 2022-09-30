// PylonSample_EMVA1288.cpp
/*
	Note: Before getting started, Basler recommends reading the "Programmer's Guide" topic
	in the pylon C++ API documentation delivered with pylon.
	If you are upgrading to a higher major version of pylon, Basler also
	strongly recommends reading the "Migrating from Previous Versions" topic in the pylon C++ API documentation.

	This sample illustrates how to test some EMVA1288-like measurements, such as linearity and SNR.
	This is intended to be a rather basic sample, focusing on methods rather than accuracy & precision.
	For color cameras, due to the Bayer filter on the sensor, the camera and test must be prepared properly to get accurate results.
	Measurements are logged in a .csv file, from which charts can be made in excel, etc.
	The camera must be prepared with all color correction features turned off,
	and it must use a pixel format which does not interpolate the Bayer pattern (eg: use BayerRG8 and not RGB8).
	The test must take into account that a color camera is essentially 3 cameras (red/green/blue), all with different responses to the light.
	Likewise, the color of the light source must be taken into account as well.
*/

#define WIN_BUILD

#ifdef WIN_BUILD
#define _CRT_SECURE_NO_WARNINGS // suppress fopen_s warnings for convinience
#endif

// Include files to use the pylon API.
#include <pylon/PylonIncludes.h>

#include <pylon/BaslerUniversalInstantCamera.h>

#include <pylon/PylonGUI.h>

#include "BayerExtract.h"
#include "AnalysisTools.h"
#include "StitchImage.h" // for convience of displaying some images

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using cout.
using namespace std;

//namespace for using the universal instant camera parameter api
using namespace Basler_UniversalCameraParams;

int main(int argc, char* argv[])
{
	// The exit code of the sample application.
	int exitCode = 0;

	// Before using any pylon methods, the pylon runtime must be initialized.
	PylonInitialize();

	// What we will measure
	double exposureTime = 0;
	uint32_t minAll = 0; // minimum pixel value in image
	uint32_t maxAll = 0;
	uint32_t avgAll = 0; // average pixel value in image
	uint32_t avgRed = 0;
	uint32_t avgGreen = 0;
	uint32_t avgBlue = 0;
	double snrAll = 0; // signal to noise ratio of the image
	double snrRed = 0;
	double snrGreen = 0;
	double snrBlue = 0;
	// How we will measure it
	// We will grab images of this size
	int64_t	width = 128;
	int64_t height = 128;
	// We will take two frames at each exposure time and average the values into one 'image'.
	CPylonImage image1;
	CPylonImage image2;
	// With color cameras, we grab two images, but also extract the red, green, blue pixels from each, and treat them as "3 cameras".
	CPylonImage RedImage1;
	CPylonImage GreenImage1;
	CPylonImage BlueImage1;
	CPylonImage RedImage2;
	CPylonImage GreenImage2;
	CPylonImage BlueImage2;
	int exposureTimeIncrementUsec = 10; // With each measurment, we will increment the exposure time
	uint32_t blackLevelCalibThreshold = 0; // Before testing, increase the black level until min pixel value is above this threshold. Use 0 to disable.
	uint32_t maxImagesToGrab = 100000; // We stop when saturation is reached. If it can't be reached, stop test after this many total images grabbed.
	// Where we will log the measurements, a csv file
	std::string csvFileName = "";
	
	try
	{
		// Get the transport layer factory.
		CTlFactory& tlFactory = CTlFactory::GetInstance();

		// Get all attached devices and exit application if no device is found.
		DeviceInfoList_t devices;

		if (tlFactory.EnumerateDevices(devices) == 0)
		{
			throw RUNTIME_EXCEPTION("Camera Not Found.");
		}

		// Create an "Instant Camera" from the first device found.
		CBaslerUniversalInstantCamera camera(tlFactory.CreateDevice(devices[0]));

		// Print the name of the device.
		cout << "Using device: " << camera.GetCameraContext() << " : " << camera.GetDeviceInfo().GetFriendlyName() << endl;

		// *************** CAMERA SETUP ******************************************************************************************
		// open the camera to configure settings.
		camera.Open();

		// Reset camera to default settings.
		camera.UserSetSelector.TrySetValue(UserSetSelectorEnums::UserSetSelector_Default);
		camera.UserSetLoad.Execute();

		// Use mono format for mono cameras, Bayer format for color cameras. Bayer is a must
		if (camera.PixelFormat.TrySetValue(PixelFormat_BayerRG8) == false)
			camera.PixelFormat.TrySetValue(PixelFormat_Mono8);
		
		// Use an AOI near the center of the image
		camera.Width.TrySetValue(width);
		camera.Height.TrySetValue(height);
		camera.OffsetX.TrySetValue(camera.SensorWidth.GetValue() / 2);
		camera.OffsetY.TrySetValue(camera.SensorHeight.GetValue() / 2);

		// We will start the test at the minimum exposure time.
		camera.ExposureTime.TrySetToMinimum();

		// For all cameras, we must make sure auto functions are off, and gain, black level, etc. are set to zero
		camera.Gain.TrySetValue(0);
		camera.Gamma.TrySetValue(1.0);
		camera.BlackLevel.TrySetValue(0);
		camera.DigitalShift.TrySetValue(0);
		camera.GainAuto.TrySetValue(GainAutoEnums::GainAuto_Off);
		camera.ExposureAuto.TrySetValue(ExposureAutoEnums::ExposureAuto_Off);

		// For color cameras, we need to turn off any color correction/processing features
		camera.BslLightSourcePreset.TrySetValue("Off");
		camera.BslLightSourcePresetFeatureSelector.TrySetValue(BslLightSourcePresetFeatureSelector_WhiteBalance);
		camera.BslLightSourcePresetFeatureEnable.TrySetValue(false);
		camera.BslLightSourcePresetFeatureSelector.TrySetValue(BslLightSourcePresetFeatureSelector_ColorTransformation);
		camera.BslLightSourcePresetFeatureEnable.TrySetValue(false);
		camera.BslLightSourcePresetFeatureSelector.TrySetValue(BslLightSourcePresetFeatureSelector_ColorAdjustment);
		camera.BslLightSourcePresetFeatureEnable.TrySetValue(false);
		camera.BslHue.TrySetValue(0);
		camera.BslSaturation.TrySetValue(1.0);
		camera.BslColorSpace.TrySetValue(BslColorSpaceEnums::BslColorSpace_Off);
		camera.BslColorAdjustmentEnable.TrySetValue(false);
		camera.ColorTransformationEnable.TrySetValue(false);
		camera.BalanceWhiteAuto.TrySetValue("Off");
		camera.BalanceRatioSelector.TrySetValue("Red");
		camera.BalanceRatio.TrySetValue(1.0);
		camera.BalanceRatioSelector.TrySetValue("Green");
		camera.BalanceRatio.TrySetValue(1.0);
		camera.BalanceRatioSelector.TrySetValue("Blue");
		camera.BalanceRatio.TrySetValue(1.0);
		
		// We will acquire images using a software trigger. FrameBurstStart is used to acquire two images per trigger.
		camera.TriggerSelector.TrySetValue(Basler_UniversalCameraParams::TriggerSelector_FrameBurstStart);
		camera.TriggerMode.TrySetValue(Basler_UniversalCameraParams::TriggerMode_On);
		camera.TriggerSource.TrySetValue(Basler_UniversalCameraParams::TriggerSource_Software);
		camera.AcquisitionBurstFrameCount.TrySetValue(2);

		// if using a Basler light, turn it on
		if (camera.BslLightControlMode.IsWritable())
		{
			camera.BslLightControlMode.TrySetValue(BslLightControlMode_On); // turn on light control
			camera.BslLightControlEnumerateDevices.TryExecute(); // enumerate connected lights 
			// the light selector feature is not readable if no lights were found
			if (camera.BslLightDeviceSelector.IsReadable() == false)
			{
				throw RUNTIME_EXCEPTION("Basler Camera Light Not Found.", __FILE__, __LINE__);
			}
			camera.BslLightDeviceSelector.TrySetValue(BslLightDeviceSelector_Device1);
			camera.BslLightDeviceBrightness.TrySetValue(1); // percent
			camera.BslLightDeviceSelector.TrySetValue(BslLightDeviceSelector_Device1);
			camera.BslLightDeviceOperationMode.TrySetValue(BslLightDeviceOperationMode_On);
		}
		// ********** END CAMERA SETUP ***********************************************************************************************

		// setup the CSV file of results
		csvFileName.append(camera.GetDeviceInfo().GetFriendlyName().c_str());
		csvFileName.append("_");
		csvFileName.append(camera.PixelFormat.ToString().c_str());
		csvFileName.append("_");
		csvFileName.append(camera.Width.ToString().c_str());
		csvFileName.append("x");
		csvFileName.append(camera.Height.ToString().c_str());
		csvFileName.append(".csv");
		std::FILE* const csvfileout = std::fopen(csvFileName.c_str(), "wb+");
		if (csvfileout == NULL)
		{
			throw GenICam::RuntimeException("CSV file already opened by another application.", __FILE__, __LINE__);
		}

		// Prepare a header for the csv file
		std::fprintf(csvfileout, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
			"Exposure Time",
			"Min Pixel Value",
			"Max Pixel Value",
			"Average All Pixels",
			"Avg Red Pixels",
			"Avg Green Pixels",
			"Avg Blue Pixels",
			"SNR All Pixels",
			"SNR Red",
			"SNR Green",
			"SNR Blue");
		std::fprintf(csvfileout, "\n");

		// find out when we should stop the test due to saturation
		int64_t saturationValue = camera.PixelDynamicRangeMax.GetValue();

		// This smart pointer will receive the grab result data.
		CGrabResultPtr ptrGrabResult1;
		CGrabResultPtr ptrGrabResult2;

		// StartGrabbing() starts the streamgrabber on the host, and starts image acquisition on the camera.
		camera.StartGrabbing();

		// Run a loop of trigger camera, grab image, process image, save data
		for (uint32_t i = 0; i < maxImagesToGrab && camera.IsGrabbing(); ++i)
		{
			// trigger the cameras
			camera.TriggerSoftware.Execute();

			// Wait for images to arrive and then retrieve them into the smartpointers
			camera.RetrieveResult(5000, ptrGrabResult1, TimeoutHandling_ThrowException);
			camera.RetrieveResult(5000, ptrGrabResult2, TimeoutHandling_ThrowException);

			// Image grabbed successfully?
			if (ptrGrabResult1->GrabSucceeded() && ptrGrabResult2->GrabSucceeded())
			{
				// Attach the "GrabResult" to a "Pylon Image" for easier handling.
				image1.AttachGrabResultBuffer(ptrGrabResult1);
				image2.AttachGrabResultBuffer(ptrGrabResult2);

				// for debugging convinience, we can stitch together and display the two images side by side.
				{
					CPylonImage stitchedImage;
					std::string err = "";
					StitchImage::StitchToRight(image1, image2, &stitchedImage, err);
#if defined WIN_BUILD
					Pylon::DisplayImage(0, stitchedImage);
#endif
				}

				// It's advised to check if we have any pixels of zero value and increase the blacklevel until we get some reading.
				if (AnalysisTools::FindMin(image1) < blackLevelCalibThreshold || AnalysisTools::FindMin(image2) < blackLevelCalibThreshold)
				{
					cout << "Zero value pixels detected, increasing blacklevel before testing..." << endl;
					camera.BlackLevel.SetValue(camera.BlackLevel.GetValue() + 1);
				}
				else
				{
					// find the average min and max pixel value
					minAll = (AnalysisTools::FindMin(image1) + AnalysisTools::FindMin(image2)) / 2;
					maxAll = (AnalysisTools::FindMax(image1) + AnalysisTools::FindMax(image2)) / 2;

					// Find the average pixel value and SNR value for the combined images
					if (IsMonoImage(ptrGrabResult1->GetPixelType()))
					{
						// Find the average of the average pixel value for both images
						avgAll = (AnalysisTools::FindAvg(image1) + AnalysisTools::FindAvg(image2)) / 2;
						// Find the average SNR of the two images
						snrAll = (AnalysisTools::FindSNR(image1) + AnalysisTools::FindSNR(image2)) / 2;
					}
					else
					{
						// We will need to extract the pixels of the bayer pattern into three images
						std::string errorMessage = "";
						if (BayerExtract::Extract(image1, RedImage1, GreenImage1, BlueImage1, errorMessage) == false)
						{
							cout << errorMessage << endl;
							return 1;
						}

						errorMessage = "";
						if (BayerExtract::Extract(image2, RedImage2, GreenImage2, BlueImage2, errorMessage) == false)
						{
							cout << errorMessage << endl;
							return 1;
						}

						// Find the average of the average pixel value for both images
						avgRed = (AnalysisTools::FindAvg(RedImage1) + AnalysisTools::FindAvg(RedImage2)) / 2;
						avgGreen = (AnalysisTools::FindAvg(GreenImage1) + AnalysisTools::FindAvg(GreenImage2)) / 2;
						avgBlue = (AnalysisTools::FindAvg(BlueImage1) + AnalysisTools::FindAvg(BlueImage2)) / 2;

						// Find the average SNR of the two images
						snrRed = (AnalysisTools::FindSNR(RedImage1) + AnalysisTools::FindAvg(RedImage2)) / 2;
						snrGreen = (AnalysisTools::FindSNR(GreenImage1) + AnalysisTools::FindAvg(GreenImage1)) / 2;
						snrBlue = (AnalysisTools::FindSNR(BlueImage1) + AnalysisTools::FindAvg(BlueImage2)) / 2;

						// Find values for the average and snr of all the pixels from the original images together.
						// Note: This illustrates why the colors must be measured individually.
						//       The response will always look non-linear if all the pixels are measured together,
						//       Even if all of the color features are disabled and pure 'white' light is used.
						//       (The different QE of the sensor under filtered light plays a role)
						avgAll = (AnalysisTools::FindAvg(image1) + AnalysisTools::FindAvg(image2)) / 2;
						snrAll = (AnalysisTools::FindSNR(image1) + AnalysisTools::FindSNR(image2)) / 2;

						// for debugging, we can also stitch together and display the extracted R,G,B sub-images of the two original images
						{
							CPylonImage stitchedImage;
							std::string err = "";
							StitchImage::StitchToRight(stitchedImage, RedImage1, &stitchedImage, err);
							StitchImage::StitchToRight(stitchedImage, GreenImage1, &stitchedImage, err);
							StitchImage::StitchToRight(stitchedImage, BlueImage1, &stitchedImage, err);
							StitchImage::StitchToRight(stitchedImage, RedImage2, &stitchedImage, err);
							StitchImage::StitchToRight(stitchedImage, GreenImage2, &stitchedImage, err);
							StitchImage::StitchToRight(stitchedImage, BlueImage2, &stitchedImage, err);
#if defined WIN_BUILD
							Pylon::DisplayImage(1, stitchedImage);
#endif
						}
					}

					// get the exposure time for this measurement
					exposureTime = camera.ExposureTime.GetValue();

					// Log the measurements into the .csv file.
					std::fprintf(csvfileout, "%f,%u,%u,%u,%u,%u,%u,%f,%f,%f,%f",
						(double)exposureTime,
						(uint32_t)minAll,
						(uint32_t)maxAll,
						(uint32_t)avgAll,
						(uint32_t)avgRed,
						(uint32_t)avgGreen,
						(uint32_t)avgBlue,
						(double)snrAll,
						(double)snrRed,
						(double)snrGreen,
						(double)snrBlue);
					std::fprintf(csvfileout, "\n");

					// Display the exposure time and avg pixel values.
					cout << std::setw(8)
						<< std::setw(8) << exposureTime << " "
						<< std::setw(8) << minAll << " "
						<< std::setw(8) << maxAll << " "
						<< std::setw(8) << avgAll << " "
						<< std::setw(8) << avgRed << " "
						<< std::setw(8) << avgGreen << " "
						<< std::setw(8) << avgBlue << " "
						<< std::setw(8) << snrAll << " "
						<< std::setw(8) << snrRed << " "
						<< std::setw(8) << snrGreen << " "
						<< std::setw(8) << snrBlue << " "
						<< endl;

					// stop if we've reached saturation
					// if you want to see what happens to linearity & snr at saturation, change this to FindMin() or FindAvg()
					if (AnalysisTools::FindMax(image1) == saturationValue && AnalysisTools::FindMax(image2) == saturationValue)
					{
						camera.StopGrabbing();
						cout << endl << "Saturation Reached. Stopping Test..." << endl;
						cout << "see \"" << csvFileName << "\" for results." << endl;
					}
					else
					{
						// Increment the exposure time for the next image.
						camera.ExposureTime.SetValue(exposureTime + exposureTimeIncrementUsec);
					}
				}
			}
			else
			{
				cout << "Error: " << std::hex << ptrGrabResult1->GetErrorCode() << std::dec << " " << ptrGrabResult1->GetErrorDescription() << endl;
				cout << "Error: " << std::hex << ptrGrabResult2->GetErrorCode() << std::dec << " " << ptrGrabResult2->GetErrorDescription() << endl;
			}
		}

		// close the csv file
		std::fclose(csvfileout);

		// For convinience, turn off the light and turn turn off triggering (if you like to go now into pylon viewer and do other things)
		if (camera.BslLightControlMode.IsWritable())
			camera.BslLightDeviceOperationMode.TrySetValue(BslLightDeviceOperationMode_Off);
		camera.TriggerSelector.TrySetValue(Basler_UniversalCameraParams::TriggerSelector_FrameBurstStart);
		camera.TriggerMode.TrySetValue(Basler_UniversalCameraParams::TriggerMode_Off);		
	}
	catch (const GenericException& e)
	{
		// Error handling
		cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
		exitCode = 1;
	}
	catch (const std::exception& e)
	{
		// Error handling
		cerr << "An exception occurred." << endl
			<< e.what() << endl;
		exitCode = 1;
	}
	catch (...)
	{
		// Error handling
		cerr << "An unkonwn exception occurred." << endl;
		exitCode = 1;
	}

	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press enter to exit." << endl;
	while (cin.get() != '\n');

	// Releases all pylon resources.
	PylonTerminate();

	return exitCode;
}
