// BayerExtract.h
// Functions to extract color channels from un-interpolated raw Bayer-formatted images.
//
// Copyright (c) 2022 Matthew Breit - matt.breit@baslerweb.com or matt.breit@gmail.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef BAYEREXTRACT_H
#define BAYEREXTRACT_H

#ifndef LINUX_BUILD
#define WIN_BUILD
#endif

#ifdef WIN_BUILD
#define _CRT_SECURE_NO_WARNINGS // suppress fopen_s warnings for convinience (if this header included first)
#endif

// We will use with Pylon
#include <pylon/PylonIncludes.h>

#include <cstdio>
#include <string>
#include <stdint.h>

namespace BayerExtract
{
	// Extract the three subimages (RGB) from the main image and place them into existing PylonImages.
	static bool Extract(Pylon::CPylonImage& image, Pylon::CPylonImage& redImage, Pylon::CPylonImage& greenImage, Pylon::CPylonImage& blueImage, std::string& errorMessage);
}

// *********************************************************************************************************
inline bool BayerExtract::Extract(Pylon::CPylonImage& image, Pylon::CPylonImage& redImage, Pylon::CPylonImage& greenImage, Pylon::CPylonImage& blueImage, std::string& errorMessage)
{
	try
	{
		Pylon::EPixelType pixelType = image.GetPixelType();

		if (Pylon::IsBayer(pixelType) == false)
		{
			errorMessage = "ERROR: Pixel type not Bayer.";
			return false;
		}

		uint32_t bitDepth = Pylon::BitDepth(pixelType);

		if (bitDepth > 8 || bitDepth < 8)
		{
			errorMessage = "ERROR: Only 8bit format is currently supported.";
			return false;
		}

		// Bayer filters have double the amount of green pixels, so we will average them to get a green sub-image.
		Pylon::CPylonImage m_GreenImage1;
		Pylon::CPylonImage m_GreenImage2;

		// Note: This section can only be used with Bayer "RG" aligned sensors.
		// The individual channel images are 1/2 the resolution of the original image, due to the bayer filter.
		redImage.Reset(Pylon::EPixelType::PixelType_Mono8, image.GetWidth() / 2, image.GetHeight() / 2);
		m_GreenImage1.Reset(Pylon::EPixelType::PixelType_Mono8, image.GetWidth() / 2, image.GetHeight() / 2);
		m_GreenImage2.Reset(Pylon::EPixelType::PixelType_Mono8, image.GetWidth() / 2, image.GetHeight() / 2);
		greenImage.Reset(Pylon::EPixelType::PixelType_Mono8, image.GetWidth() / 2, image.GetHeight() / 2);
		blueImage.Reset(Pylon::EPixelType::PixelType_Mono8, image.GetWidth() / 2, image.GetHeight() / 2);

		// Get pointers to the buffers
		uint8_t* pBuffer = (uint8_t*)image.GetBuffer();
		uint8_t* pRedImage = (uint8_t*)redImage.GetBuffer();
		uint8_t* pGreenImage1 = (uint8_t*)m_GreenImage1.GetBuffer();
		uint8_t* pGreenImage2 = (uint8_t*)m_GreenImage2.GetBuffer();
		uint8_t* pGreenImage = (uint8_t*)greenImage.GetBuffer();
		uint8_t* pBlueImage = (uint8_t*)blueImage.GetBuffer();

		int i = 0;
		int j = 0;
		for (uint32_t y = 0; y < image.GetHeight(); y++)
		{
			for (uint32_t x = 0; x < image.GetWidth(); x++)
			{
				uint32_t loc = (y * image.GetWidth() + x);

				if (y % 2 == 0)
				{
					if (x % 2 == 0)
					{
						if (pixelType == Pylon::EPixelType::PixelType_BayerRG8)
						{
							pRedImage[i] = pBuffer[loc];
							pGreenImage1[i] = pBuffer[loc + 1];
						}
						else if (pixelType == Pylon::EPixelType::PixelType_BayerGR8)
						{
							pGreenImage1[i] = pBuffer[loc];
							pRedImage[i] = pBuffer[loc + 1];
						}

						i++;
					}
				}
				else
				{
					if (x % 2 == 0)
					{
						if (pixelType == Pylon::EPixelType::PixelType_BayerRG8)
						{
							pGreenImage2[j] = pBuffer[loc];
							pBlueImage[j] = pBuffer[loc + 1];
						}
						else if (pixelType == Pylon::EPixelType::PixelType_BayerGR8)
						{
							pBlueImage[i] = pBuffer[loc];
							pGreenImage2[i] = pBuffer[loc + 1];
						}

						j++;
					}
				}
			}
		}
		for (uint32_t i = 0; i < greenImage.GetImageSize(); i++)
		{
			pGreenImage[i] = (pGreenImage1[i] + pGreenImage2[i]) / 2;
		}


		return true;
	}
	catch (std::exception& e)
	{
		errorMessage = "An exception occured in Extract(): ";
		errorMessage.append(e.what());
		return false;
	}
}
// *********************************************************************************************************
#endif