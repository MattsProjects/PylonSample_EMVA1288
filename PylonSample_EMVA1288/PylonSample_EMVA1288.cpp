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

		// Create an array of instant cameras for the found devices and avoid exceeding a maximum number of devices.
		CBaslerUniversalInstantCamera camera(tlFactory.CreateFirstDevice());

		// Print the name of the device.
		cout << "Using device: " << camera.GetCameraContext() << " : " << camera.GetDeviceInfo().GetFriendlyName() << endl;

		// open the camera to configure settings.
		camera.Open();

		// *************** TEST PARAMETERS SECTION ******************************************************************************************
		// If you like to test the camera at default settings, this is the code to load them.
		camera.UserSetSelector.TrySetValue(UserSetSelectorEnums::UserSetSelector_Default);
		camera.UserSetLoad.Execute();

		// For mono cameras, use mono8, for color cameras, use Bayer format
	    if (camera.PixelFormat.TrySetValue(PixelFormat_BayerRG8) != true)
			camera.PixelFormat.TrySetValue(PixelFormat_Mono8);

		//use an AOI near the center of the image
		camera.Width.TrySetValue(128);
		camera.Height.TrySetValue(128);
		camera.OffsetX.TrySetValue(camera.SensorWidth.GetValue() / 2);
		camera.OffsetY.TrySetValue(camera.SensorHeight.GetValue() / 2);

		// We will start the test at the minimum exposure time.
		camera.ExposureTime.TrySetToMinimum();

		// With each image, we will increment the exposure time
		int exposureTimeIncrementUsec = 50;

		// We will aim to stop the test when all pixels are saturated, but if this is not possible, grab maximum this amount of images.
		uint32_t maxImagesToGrab = 100000;

		// This section turns off all of the color correction features, as well as gain, black level, gamma, auto WB/gain/Exposure etc.
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
		camera.Gain.TrySetValue(0);
		camera.Gamma.TrySetValue(1.0);
		camera.BlackLevel.TrySetValue(0);
		camera.DigitalShift.TrySetValue(0);
		camera.GainAuto.TrySetValue(GainAutoEnums::GainAuto_Off);
		camera.ExposureAuto.TrySetValue(ExposureAutoEnums::ExposureAuto_Off);

		// *********************************************************************************************************

		// For the test, we will acquire 2 images per trigger (EMVA1288 calls for average of 2 images per exposure time)
		camera.TriggerSelector.TrySetValue(Basler_UniversalCameraParams::TriggerSelector_FrameBurstStart);
		camera.TriggerMode.TrySetValue(Basler_UniversalCameraParams::TriggerMode_On);
		camera.TriggerSource.TrySetValue(Basler_UniversalCameraParams::TriggerSource_Software);
		camera.AcquisitionBurstFrameCount.TrySetValue(2);

		// if using a Basler light, turn it on
		camera.BslLightControlMode.TrySetValue(BslLightControlMode_On);
		camera.BslLightControlEnumerateDevices.TryExecute();
		camera.BslLightDeviceSelector.TrySetValue(BslLightDeviceSelector_Device1);
		camera.BslLightDeviceBrightness.TrySetValue(1.0);
		camera.BslLightDeviceSelector.TrySetValue(BslLightDeviceSelector_Device1);
		camera.BslLightDeviceOperationMode.TrySetValue(BslLightDeviceOperationMode_On);

		// What we will measure
		double exposureTime = 0;
		uint32_t minAll = 0;
		uint32_t maxAll = 0;
		uint32_t avgAll = 0;
		uint32_t avgRed = 0;
		uint32_t avgGreen = 0;
		uint32_t avgBlue = 0;
		double snrAll = 0;
		double snrRed = 0;
		double snrGreen = 0;
		double snrBlue = 0;
				
		// we will stop the test when we've reached saturation
		int64_t saturationValue = camera.PixelDynamicRangeMax.GetValue();

		// Where we will log the measurements
		std::string csvFileName = "";
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
			cout << "CSV file already opened by another application." << endl;
			return 1;
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

		// This smart pointer will receive the grab result data.
		CGrabResultPtr ptrGrabResult1;
		CGrabResultPtr ptrGrabResult2;

		// Start the streamgrabber
		camera.StartGrabbing();

		// Run a loop of trigger camera, grab image, process image, save data
		for (uint32_t i = 0; i < maxImagesToGrab && camera.IsGrabbing(); ++i)
		{
			// trigger the cameras
			camera.TriggerSoftware.Execute();

			// Wait for an image to arrive and then retrieve it
			camera.RetrieveResult(5000, ptrGrabResult1, TimeoutHandling_ThrowException);
			camera.RetrieveResult(5000, ptrGrabResult2, TimeoutHandling_ThrowException);

			// Image grabbed successfully?
			if (ptrGrabResult1->GrabSucceeded() && ptrGrabResult2->GrabSucceeded())
			{
				CPylonImage image1;
				CPylonImage image2;
				image1.AttachGrabResultBuffer(ptrGrabResult1);
				image2.AttachGrabResultBuffer(ptrGrabResult2);

				minAll = (AnalysisTools::FindMin(image1) + AnalysisTools::FindMin(image2)) / 2;
				maxAll = (AnalysisTools::FindMax(image1) + AnalysisTools::FindMax(image2)) / 2;

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
					CPylonImage RedImage1;
					CPylonImage GreenImage1;
					CPylonImage BlueImage1;
					CPylonImage RedImage2;
					CPylonImage GreenImage2;
					CPylonImage BlueImage2;

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
				}

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
				// Really we should stop testing once any pixel reaches saturation,
				// but for this sample we stop when all pixels reach saturation,
				// to illustrate what happens when the response is non-linear.
				if (AnalysisTools::FindMin(image1) == saturationValue && AnalysisTools::FindMin(image2) == saturationValue)
				{
					camera.StopGrabbing();
					cout << "All pixels saturated. Stopping Test..." << endl;
				}
				else
				{
					// Increment the exposure time for the next image.
					camera.ExposureTime.SetValue(exposureTime + exposureTimeIncrementUsec);
				}
				//cin.get();
			}
			else
			{
				cout << "Error: " << std::hex << ptrGrabResult1->GetErrorCode() << std::dec << " " << ptrGrabResult1->GetErrorDescription() << endl;
				cout << "Error: " << std::hex << ptrGrabResult2->GetErrorCode() << std::dec << " " << ptrGrabResult2->GetErrorDescription() << endl;
			}
		}

		// close the csv file
		std::fclose(csvfileout);

		// turn off triggering (for convinence if you like to go now into pylon viewer and grab images easier)
		camera.TriggerSelector.TrySetValue(Basler_UniversalCameraParams::TriggerSelector_FrameBurstStart);
		camera.TriggerMode.TrySetValue(Basler_UniversalCameraParams::TriggerMode_Off);

		// turn off the light
		camera.BslLightDeviceSelector.TrySetValue(BslLightDeviceSelector_Device1);
		camera.BslLightDeviceOperationMode.TrySetValue(BslLightDeviceOperationMode_Off);
	}
	catch (const GenericException& e)
	{
		// Error handling
		cerr << "An exception occurred." << endl
			<< e.GetDescription() << endl;
		exitCode = 1;
	}

	// Comment the following two lines to disable waiting on exit.
	cerr << endl << "Press enter to exit." << endl;
	while (cin.get() != '\n');

	// Releases all pylon resources.
	PylonTerminate();

	return exitCode;
}
