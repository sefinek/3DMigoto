#include "Hunting.h"

#include <string>
#include <D3Dcompiler.h>
#include <codecvt>

#include "ScreenGrab.h"
#include "wincodec.h"

#include "util.h"
#include "DecompileHLSL.h"
#include "Input.h"
#include "Override.h"
#include "Globals.h"
#include "IniHandler.h"
#include "D3D_Shaders\stdafx.h"

// bo3b: For this routine, we have a lot of warnings in x64, from converting a size_t result into the needed
//  DWORD type for the Write calls.  These are writing 256 byte strings, so there is never a chance that it 
//  will lose data, so rather than do anything heroic here, I'm just doing type casts on the strlen function.

DWORD castStrLen(const char* string)
{
	return (DWORD)strlen(string);
}

static void DumpUsageResourceInfo(HANDLE f, std::set<uint32_t> *hashes, char *tag)
{
	std::set<uint32_t>::iterator orig_hash;
	std::set<uint32_t>::iterator iCopy;
	std::set<UINT>::iterator iMU;
	CopySubresourceRegionContaminationMap::iterator iRegion;
	CopySubresourceRegionContaminationMap::key_type kRegion;
	CopySubresourceRegionContamination *region;

	uint32_t srcHash;
	UINT SrcIdx, SrcMip, DstIdx, DstMip;
	struct ResourceHashInfo *info;
	char buf[256];
	DWORD written; // Really? A >required< "optional" paramter that we don't care about?
	bool nl;

	for (orig_hash = hashes->begin(); orig_hash != hashes->end(); orig_hash++) {
		try {
			info = &G->mResourceInfo.at(*orig_hash);
		} catch (std::out_of_range) {
			continue;
		}
		_snprintf_s(buf, 256, 256, "<%s orig_hash=%08lx ", tag, *orig_hash);
		WriteFile(f, buf, castStrLen(buf), &written, 0);
		StrResourceDesc(buf, 256, *info);
		WriteFile(f, buf, castStrLen(buf), &written, 0);

		if (info->hash_contaminated) {
			_snprintf_s(buf, 256, 256, " hash_contaminated=true");
			WriteFile(f, buf, castStrLen(buf), &written, 0);
		}

		WriteFile(f, ">", 1, &written, 0);
		nl = false;

		for (iMU = info->update_contamination.begin(); iMU != info->update_contamination.end(); iMU++) {
			_snprintf_s(buf, 256, 256, "\n  <UpdateSubresource subresource=%u></UpdateSubresource>", *iMU);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			nl = true;
		}
		for (iMU = info->map_contamination.begin(); iMU != info->map_contamination.end(); iMU++) {
			_snprintf_s(buf, 256, 256, "\n  <CPUWrite subresource=%u></CPUWrite>", *iMU);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			nl = true;
		}
		for (iCopy = info->copy_contamination.begin(); iCopy != info->copy_contamination.end(); iCopy++) {
			_snprintf_s(buf, 256, 256, "\n  <CopiedFrom>%08lx</CopiedFrom>", *iCopy);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			nl = true;
		}
		for (iRegion = info->region_contamination.begin(); iRegion != info->region_contamination.end(); iRegion++) {
			kRegion = iRegion->first;
			region = &iRegion->second;

			srcHash = std::get<0>(kRegion);
			DstIdx = std::get<1>(kRegion);
			DstMip = std::get<2>(kRegion);
			SrcIdx = std::get<3>(kRegion);
			SrcMip = std::get<4>(kRegion);

			_snprintf_s(buf, 256, 256, "\n  <SubresourceCopiedFrom partial=");
			WriteFile(f, buf, castStrLen(buf), &written, 0);

			if (region->partial) {
				_snprintf_s(buf, 256, 256, "true");
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			} else {
				_snprintf_s(buf, 256, 256, "false");
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}

			if (DstIdx || SrcIdx) {
				_snprintf_s(buf, 256, 256, " DstIdx=%u SrcIdx=%u",
						DstIdx, SrcIdx);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}

			if (DstMip || SrcMip) {
				_snprintf_s(buf, 256, 256, " DstMip=%u SrcMip=%u",
						DstMip, SrcMip);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}

			if (region->DstX || region->DstY || region->DstZ) {
				_snprintf_s(buf, 256, 256, " DstX=%u DstY=%u DstZ=%u",
						region->DstX, region->DstY, region->DstZ);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}

			if (region->SrcBox.left || region->SrcBox.right != UINT_MAX) {
				_snprintf_s(buf, 256, 256, " SrcLeft=%u SrcRight=%u",
					region->SrcBox.left, region->SrcBox.right);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}
			if (region->SrcBox.top || region->SrcBox.bottom != UINT_MAX) {
				_snprintf_s(buf, 256, 256, " SrcTop=%u SrcBottom=%u",
					region->SrcBox.top, region->SrcBox.bottom);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}
			if (region->SrcBox.front || region->SrcBox.back != UINT_MAX) {
				_snprintf_s(buf, 256, 256, " SrcFront=%u SrcBack=%u",
					region->SrcBox.front, region->SrcBox.back);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}

			_snprintf_s(buf, 256, 256, ">%08lx</SubresourceCopiedFrom>", srcHash);
			WriteFile(f, buf, castStrLen(buf), &written, 0);

			nl = true;
		}

		if (nl)
			WriteFile(f, "\n", castStrLen("\n"), &written, 0);

		_snprintf_s(buf, 256, 256, "</%s>\n", tag);
		WriteFile(f, buf, castStrLen(buf), &written, 0);
	}
}

static void DumpUsageRegister(HANDLE f, char *tag, int id, const ResourceSnapshot &info)
{
	char buf[256];
	DWORD written;

	sprintf(buf, "  <%s", tag);
	WriteFile(f, buf, castStrLen(buf), &written, 0);

	if (id != -1) {
		sprintf(buf, " id=%d", id);
		WriteFile(f, buf, castStrLen(buf), &written, 0);
	}

	sprintf(buf, " handle=%p", info.handle);
	WriteFile(f, buf, castStrLen(buf), &written, 0);

	if (info.orig_hash != info.hash) {
		sprintf(buf, " orig_hash=%08lx", info.orig_hash);
		WriteFile(f, buf, castStrLen(buf), &written, 0);
	}

	try {
		if (G->mResourceInfo.at(info.orig_hash).hash_contaminated) {
			sprintf(buf, " hash_contaminated=true");
			WriteFile(f, buf, castStrLen(buf), &written, 0);
		}
	} catch (std::out_of_range) {
	}

	sprintf(buf, ">%08lx</%s>\n", info.hash, tag);
	WriteFile(f, buf, castStrLen(buf), &written, 0);
}

static void DumpShaderUsageInfo(HANDLE f, std::map<UINT64, ShaderInfoData> *info_map, char *tag)
{
	std::map<UINT64, ShaderInfoData>::iterator i;
	std::set<UINT64>::iterator j;
	std::map<int, std::set<ResourceSnapshot>>::const_iterator k;
	std::set<ResourceSnapshot>::const_iterator o;
	std::vector<std::set<ResourceSnapshot>>::iterator m;
	std::set<ResourceSnapshot>::iterator n;
	char buf[256];
	DWORD written;
	int pos;

	for (i = info_map->begin(); i != info_map->end(); ++i) {
		sprintf(buf, "<%s hash=\"%016llx\">\n", tag, i->first);
		WriteFile(f, buf, castStrLen(buf), &written, 0);

		// Does not apply to compute shaders:
		if (!i->second.PeerShaders.empty()) {
			const char *PEER_HEADER = "  <PeerShaders>";
			WriteFile(f, PEER_HEADER, castStrLen(PEER_HEADER), &written, 0);

			for (j = i->second.PeerShaders.begin(); j != i->second.PeerShaders.end(); ++j) {
				sprintf(buf, "%016llx ", *j);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}
			const char *REG_HEADER = "</PeerShaders>\n";
			WriteFile(f, REG_HEADER, castStrLen(REG_HEADER), &written, 0);
		}

		for (k = i->second.ResourceRegisters.begin(); k != i->second.ResourceRegisters.end(); ++k) {
			for (o = k->second.begin(); o != k->second.end(); o++)
				DumpUsageRegister(f, "Register", k->first, *o);
		}

		// Only applies to pixel shaders:
		for (m = i->second.RenderTargets.begin(), pos = 0; m != i->second.RenderTargets.end(); m++, pos++) {
			for (o = (*m).begin(); o != (*m).end(); o++)
				DumpUsageRegister(f, "RenderTarget", pos, *o);
		}

		// Only applies to pixel shaders:
		for (n = i->second.DepthTargets.begin(); n != i->second.DepthTargets.end(); n++) {
			DumpUsageRegister(f, "DepthTarget", -1, *n);
		}

		// Applies to pixel and compute shaders:
		for (k = i->second.UAVs.begin(); k != i->second.UAVs.end(); ++k) {
			for (o = k->second.begin(); o != k->second.end(); o++)
				DumpUsageRegister(f, "UAV", k->first, *o);
		}

		sprintf(buf, "</%s>\n", tag);
		WriteFile(f, buf, castStrLen(buf), &written, 0);
	}
}

// Expects the caller to have entered the critical section.
void DumpUsage(wchar_t *dir)
{
	wchar_t path[MAX_PATH];
	if (dir) {
		wcscpy(path, dir);
		wcscat(path, L"\\");
	} else {
		if (!GetModuleFileName(0, path, MAX_PATH))
			return;
		wcsrchr(path, L'\\')[1] = 0;
	}
	wcscat(path, L"ShaderUsage.txt");
	HANDLE f = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		LogInfo("Error dumping ShaderUsage.txt\n");
		return;
	}

	DumpShaderUsageInfo(f, &G->mVertexShaderInfo, "VertexShader");
	DumpShaderUsageInfo(f, &G->mHullShaderInfo, "HullShader");
	DumpShaderUsageInfo(f, &G->mDomainShaderInfo, "DomainShader");
	DumpShaderUsageInfo(f, &G->mGeometryShaderInfo, "GeometryShader");
	DumpShaderUsageInfo(f, &G->mPixelShaderInfo, "PixelShader");
	DumpShaderUsageInfo(f, &G->mComputeShaderInfo, "ComputeShader");

	DumpUsageResourceInfo(f, &G->mRenderTargetInfo, "RenderTarget");
	DumpUsageResourceInfo(f, &G->mDepthTargetInfo, "DepthTarget");
	DumpUsageResourceInfo(f, &G->mUnorderedAccessInfo, "UAV");
	DumpUsageResourceInfo(f, &G->mShaderResourceInfo, "Register");
	DumpUsageResourceInfo(f, &G->mCopiedResourceInfo, "CopySource");
	CloseHandle(f);
}


// Make a snapshot of the backbuffer, with the current shader disabled, as a good piece
// of documentation.  The name will include the hash code, making a direct shader reference.
//
// CoInitialize must be called for WIC to work.  We can call it multiple times, it will
// return the S_FALSE if it's already inited.

static void SimpleScreenShot(HackerDevice *pDevice, UINT64 hash, wstring shaderType)
{
	wchar_t fullName[MAX_PATH];
	ID3D11Texture2D *backBuffer;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
		LogInfo("*** Overlay call CoInitializeEx failed: %d\n", hr);

	hr = pDevice->GetHackerSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
	if (SUCCEEDED(hr))
	{
		swprintf_s(fullName, MAX_PATH, L"%ls\\%016llx-%ls.jpg", G->SHADER_PATH, hash, shaderType.c_str());
		hr = DirectX::SaveWICTextureToFile(pDevice->GetPassThroughOrigContext1(), backBuffer, GUID_ContainerFormatJpeg, fullName);
		backBuffer->Release();
	}

	LogInfoW(L"  SimpleScreenShot on Mark: %s, result: %d\n", fullName, hr);
}

// Similar to above, but this version enables the reverse stereo blit in nvapi
// to get the second back buffer and create a stereo 3D JPS:

static void StereoScreenShot(HackerDevice *pDevice, UINT64 hash, wstring shaderType)
{
	wchar_t fullName[MAX_PATH];
	ID3D11Texture2D *backBuffer = NULL;
	ID3D11Texture2D *stereoBackBuffer = NULL;
	D3D11_TEXTURE2D_DESC desc;
	D3D11_BOX srcBox;
	UINT srcWidth;
	HRESULT hr;
	NvAPI_Status nvret;

	hr = pDevice->GetHackerSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	if (FAILED(hr))
		return;

	backBuffer->GetDesc(&desc);

	// Intermediate resource should be 2x width to receive a stereo image:
	srcWidth = desc.Width;
	desc.Width = srcWidth * 2;

	hr = pDevice->GetPassThroughOrigDevice1()->CreateTexture2D(&desc, NULL, &stereoBackBuffer);
	if (FAILED(hr)) {
		LogInfo("StereoScreenShot failed to create intermediate texture resource: 0x%x\n", hr);
		return;
	}

	nvret = NvAPI_Stereo_ReverseStereoBlitControl(pDevice->mStereoHandle, true);
	if (nvret != NVAPI_OK) {
		LogInfo("StereoScreenShot failed to enable reverse stereo blit\n");
		goto out;
	}

	// Set the source box as per the nvapi documentation:
	srcBox.left = 0;
	srcBox.top = 0;
	srcBox.front = 0;
	srcBox.right = srcWidth;
	srcBox.bottom = desc.Height;
	srcBox.back = 1;

	// NVAPI documentation hasn't been updated to indicate which is the
	// correct function to use for the reverse stereo blit in DX11...
	// Fortunately there was really only one possibility, which is:
	pDevice->GetPassThroughOrigContext1()->CopySubresourceRegion(stereoBackBuffer, 0, 0, 0, 0, backBuffer, 0, &srcBox);

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
		LogInfo("*** Overlay call CoInitializeEx failed: %d\n", hr);

	wsprintf(fullName, L"%ls\\%016I64x-%ls.jps", G->SHADER_PATH, hash, shaderType.c_str());
	hr = DirectX::SaveWICTextureToFile(pDevice->GetPassThroughOrigContext1(), stereoBackBuffer, GUID_ContainerFormatJpeg, fullName);

	LogInfoW(L"  StereoScreenShot on Mark: %s, result: %d\n", fullName, hr);

	NvAPI_Stereo_ReverseStereoBlitControl(pDevice->mStereoHandle, false);
out:
	stereoBackBuffer->Release();
}



//--------------------------------------------------------------------------------------------------

// Write the decompiled text as HLSL source code to the txt file.
// Now also writing the ASM text to the bottom of the file, commented out.
// This keeps the ASM with the HLSL for reference and should be more convenient.
//
// This will not overwrite any file that is already there. 
// The assumption is that the shaderByteCode that we have here is always the most up to date,
// and thus is not different than the file on disk.
// If a file was already extant in the ShaderFixes, it will be picked up at game launch as the master shaderByteCode.

static bool WriteHLSL(string hlslText, string asmText, UINT64 hash, wstring shaderType)
{
	wchar_t fullName[MAX_PATH];
	FILE *fw;

	swprintf_s(fullName, MAX_PATH, L"%ls\\%016llx-%ls_replace.txt", G->SHADER_PATH, hash, shaderType.c_str());
	_wfopen_s(&fw, fullName, L"rb");
	if (fw)
	{
		LogOverlayW(LOG_INFO, L"marked shader file already exists: %s\n", fullName);
		fclose(fw);
		_wfopen_s(&fw, fullName, L"ab");
		if (fw) {
			fprintf_s(fw, " ");					// Touch file to update mod date as a convenience.
			fclose(fw);
		}
		return true;
	}

	wfopen_ensuring_access(&fw, fullName, L"wb");
	if (!fw)
	{
		LogInfoW(L"    error storing marked shader to %s\n", fullName);
		return false;
	}

	LogInfoW(L"    storing patched shader to %s\n", fullName);

	fwrite(hlslText.c_str(), 1, hlslText.size(), fw);

	fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	fwrite(asmText.c_str(), 1, asmText.size(), fw);
	fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

	fclose(fw);
	return true;
}

// This is pretty heavyweight obviously, so it is only being done during Mark operations.
// Todo: another copy/paste job, we really need some subroutines, utility library.

static string Decompile(ID3DBlob* pShaderByteCode, string asmText)
{
	LogInfo("    creating HLSL representation.\n");

	bool patched = false;
	string shaderModel;
	bool errorOccurred = false;

	// TODO: Refactor all parameters we just copy from globals into their
	// own struct so we don't have to copy all this junk
	ParseParameters p;
	p.bytecode = pShaderByteCode->GetBufferPointer();
	p.decompiled = asmText.c_str();
	p.decompiledSize = asmText.size();
	p.StereoParamsReg = G->StereoParamsReg;
	p.IniParamsReg = G->IniParamsReg;
	p.recompileVs = G->FIX_Recompile_VS;
	p.fixSvPosition = G->FIX_SV_Position;
	p.ZRepair_Dependencies1 = G->ZRepair_Dependencies1;
	p.ZRepair_Dependencies2 = G->ZRepair_Dependencies2;
	p.ZRepair_DepthTexture1 = G->ZRepair_DepthTexture1;
	p.ZRepair_DepthTexture2 = G->ZRepair_DepthTexture2;
	p.ZRepair_DepthTextureReg1 = G->ZRepair_DepthTextureReg1;
	p.ZRepair_DepthTextureReg2 = G->ZRepair_DepthTextureReg2;
	p.ZRepair_ZPosCalc1 = G->ZRepair_ZPosCalc1;
	p.ZRepair_ZPosCalc2 = G->ZRepair_ZPosCalc2;
	p.ZRepair_PositionTexture = G->ZRepair_PositionTexture;
	p.ZRepair_DepthBuffer = (G->ZBufferHashToInject != 0);
	p.ZRepair_WorldPosCalc = G->ZRepair_WorldPosCalc;
	p.BackProject_Vector1 = G->BackProject_Vector1;
	p.BackProject_Vector2 = G->BackProject_Vector2;
	p.ObjectPos_ID1 = G->ObjectPos_ID1;
	p.ObjectPos_ID2 = G->ObjectPos_ID2;
	p.ObjectPos_MUL1 = G->ObjectPos_MUL1;
	p.ObjectPos_MUL2 = G->ObjectPos_MUL2;
	p.MatrixPos_ID1 = G->MatrixPos_ID1;
	p.MatrixPos_MUL1 = G->MatrixPos_MUL1;
	p.InvTransforms = G->InvTransforms;
	p.fixLightPosition = G->FIX_Light_Position;
	p.ZeroOutput = false;
	const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);

	if (!decompiledCode.size())
	{
		LogInfo("    error while decompiling.\n");
	}

	return decompiledCode;
}



// Compile a new shader from  HLSL text input, and report on errors if any.
// Return the binary blob of pCode to be activated with CreateVertexShader or CreatePixelShader.
// If the timeStamp has not changed from when it was loaded, skip the recompile, and return false as not an 
// error, but skipped.  On actual errors, return true so that we bail out.

// Compile example taken from: http://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx

static bool RegenerateShader(wchar_t *shaderFixPath, wchar_t *fileName, const char *shaderModel, 
	UINT64 hash, wstring shaderType, ID3DBlob *origByteCode,
	__out FILETIME* timeStamp, __out wstring &headerLine, _Outptr_ ID3DBlob** pCode)
{
	*pCode = nullptr;
	wchar_t fullName[MAX_PATH];
	char apath[MAX_PATH];
	swprintf_s(fullName, MAX_PATH, L"%s\\%s", shaderFixPath, fileName);

	WarnIfConflictingShaderExists(fullName);

	HANDLE f = CreateFile(fullName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		LogInfo("    ReloadShader shader not found: %ls\n", fullName);

		return true;
	}


	DWORD srcDataSize = GetFileSize(f, 0);
	vector<char> srcData(srcDataSize);
	DWORD readSize;
	FILETIME curFileTime;

	if (!ReadFile(f, srcData.data(), srcDataSize, &readSize, 0)
		|| !GetFileTime(f, NULL, NULL, &curFileTime)
		|| srcDataSize != readSize)
	{
		LogInfo("    Error reading txt file.\n");
		CloseHandle(f);

		return true;
	}
	CloseHandle(f);

	// Check file time stamp, and only recompile shaders that have been edited since they were loaded.
	// This dramatically improves the F10 reload speed.
	if (!CompareFileTime(timeStamp, &curFileTime))
	{
		return false;
	}
	*timeStamp = curFileTime;

	// Now that we are sure to be reloading, let's see if it's an ASM file and assemble instead.
	ID3DBlob* pByteCode = nullptr;
	
	if (wcsstr(fileName, L"_replace"))
	{
		LogInfo("   >Replacement shader found. Re-Loading replacement HLSL code from %ls\n", fileName);
		LogInfo("    Reload source code loaded. Size = %d\n", srcDataSize);
		LogInfo("    compiling replacement HLSL code with shader model %s\n", shaderModel);

		// TODO: Add #defines for StereoParams and IniParams

		ID3DBlob* pErrorMsgs = nullptr;
		// Pass the real filename and use the standard include handler so that
		// #include will work with a relative path from the shader itself.
		// Later we could add a custom include handler to track dependencies so
		// that we can make reloading work better when using includes:
		wcstombs(apath, fullName, MAX_PATH);
		HRESULT ret = D3DCompile(srcData.data(), srcDataSize, apath, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", shaderModel, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pByteCode, &pErrorMsgs);

		LogInfo("    compile result for replacement HLSL shader: %x\n", ret);

		if (pErrorMsgs)
		{
			LPVOID errMsg = pErrorMsgs->GetBufferPointer();
			SIZE_T errSize = pErrorMsgs->GetBufferSize();
			LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
			if (FAILED(ret))
			{
				// If there are errors they go to the overlay
				LogOverlay(LOG_NOTICE, "%*s\n", errSize, errMsg);
			}
			else if (LogFile)
			{
				// If there are only warnings they go to the
				// log file, because it's too noisy to send all
				// these to the overlay.
				fwrite(errMsg, 1, errSize - 1, LogFile);
			}
			LogInfo("---------------------------------------------- END ----------------------------------------------\n");
			pErrorMsgs->Release();
		}

		if (FAILED(ret))
		{
			if (pByteCode)
			{
				pByteCode->Release();
				pByteCode = 0;
			}
			return true;
		}
	}
	else
	{
		LogInfo("   >Replacement shader found. Re-Loading replacement ASM code from %ls\n", fileName);
		LogInfo("    Reload source code loaded. Size = %d\n", srcDataSize);
		LogInfo("    assembling replacement ASM code with shader model %s\n", shaderModel);

		// We need original byte code unchanged, so make a copy.
		vector<byte> byteCode(origByteCode->GetBufferSize());
		memcpy(byteCode.data(), origByteCode->GetBufferPointer(), origByteCode->GetBufferSize());

		// Not completely clear what error reporting happens from assembler.  
		// We know it throws at least one exception, let's use that.
		try
		{
			byteCode = AssembleFluganWithOptionalSignatureParsing(&srcData, G->assemble_signature_comments, &byteCode);
		}
		catch (...)
		{ 
			LogInfo("  *** assembler failed.\n");
			return true;
		}

		// Since the re-assembly worked, let's make it the active shader code.
		HRESULT ret = D3DCreateBlob(byteCode.size(), &pByteCode);
		if (SUCCEEDED(ret)) {
			memcpy(pByteCode->GetBufferPointer(), byteCode.data(), byteCode.size());
		}
		else {
			LogInfo("    *** failed to allocate new Blob for assemble.\n");
			return true;
		}
	}


	// No longer generating .bin cache shaders here.  Unnecessary complexity, and this needs
	// to be refactored to use a single shader loading code anyway.


	// For success, let's add the first line of text from the file to the OriginalShaderInfo,
	// so the ShaderHacker can edit the line and reload and have it live.
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf8_to_utf16;
	headerLine = utf8_to_utf16.from_bytes(srcData.data(), strchr(srcData.data(), '\n'));

	// pCode on return == NULL for error cases, valid if made it this far.
	*pCode = pByteCode;

	return true;
}


// Strategy: When the user hits F10 as the reload key, we want to reload all of the hand-patched shaders in
//	the ShaderFixes folder, and make them live in game.  That will allow the user to test out fixes on the 
//	HLSL .txt files and do live experiments with fixes.  This makes it easier to figure things out.
//	To do that, we need to patch what is being sent to VSSetShader and PSSetShader, as they get activated
//	in game.  Since the original shader is already on the GPU from CreateVertexShader and CreatePixelShader,
//	we need to override it on the fly. We cannot change what the game originally sent as the shader request,
//	nor their storage location, because we cannot guarantee that they didn't make a copy and use it. So, the
//	item to go on is the ID3D11VertexShader* that the game received back from CreateVertexShader and will thus
//	use to activate it with VSSetShader.
//	To do the override, we've made a new Map that maps the original ID3D11VertexShader* to the new one, and
//	in VSSetShader, we will look for a map match, and if we find it, we'll apply the override of the newly
//	loaded shader.
//	Here in ReloadShader, we need to set up to make that possible, by filling in the mReloadedVertexShaders
//	map with <old,new>. In this spot, we have been notified by the user via F10 or whatever input that they
//	want the shaders reloaded. We need to take each shader hlsl.txt file, recompile it, call CreateVertexShader
//	on it to make it available in the GPU, and save the new ID3D11VertexShader* in our <old,new> map. We get the
//	old ID3D11VertexShader* reference by looking that up in the complete mVertexShaders map, by using the hash
//	number we can get from the file name itself.
//	Notably, if the user does multiple iterations, we'll still only use the same number of overrides, because
//	the map will replace the last one. This probably does leak vertex shaders on the GPU though.

// Todo: this is not a particularly good spot for the routine.  Need to move these compile/dissassemble routines
//	including those in Direct3D11Device.h into a separate file and include a .h file.
//	This routine plagarized from ReplaceShaders.

// Reload all of the patched shaders from the ShaderFixes folder, and add them to the override map, so that the
// new version will be used at VSSetShader and PSSetShader.
// File names are uniform in the form: 3c69e169edc8cd5f-ps_replace.txt

static bool ReloadShader(wchar_t *shaderPath, wchar_t *fileName, HackerDevice *device)
{
	UINT64 hash;
	ShaderOverrideMap::iterator override;
	ID3D11DeviceChild* oldShader = NULL;
	ID3D11DeviceChild* replacement = NULL;
	ID3D11ClassLinkage* classLinkage;
	ID3DBlob* shaderCode;
	string shaderModel;
	wstring shaderType;		// "vs", "ps", "cs" maybe "gs"
	wstring headerLine;		// First line of the HLSL file.
	FILETIME timeStamp;
	HRESULT hr = E_FAIL;
	bool rc = true;

	// Extract hash from first 16 characters of file name so we can look up details by hash
	wstring ws = fileName;
	hash = stoull(ws.substr(0, 16), NULL, 16);

	// This is probably unnecessary, because we modify already existing map entries, but
	// for consistency, we'll wrap this.
	EnterCriticalSection(&G->mCriticalSection);

	// Find the original shader bytecode in the mReloadedShaders Map. This map contains entries for all
	// shaders from the ShaderFixes and ShaderCache folder, and can also include .bin files that were loaded directly.
	// We include ShaderCache because that allows moving files into ShaderFixes as they are identified.
	// This needs to use the value to find the key, so a linear search.
	// It's notable that the map can contain multiple copies of the same hash, used for different visual
	// items, but with same original code.  We need to update all copies.
	for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> iter in G->mReloadedShaders)
	{
		if (iter.second.hash == hash)
		{
			oldShader = iter.first;
			classLinkage = iter.second.linkage;
			shaderModel = iter.second.shaderModel;
			shaderType = iter.second.shaderType;
			timeStamp = iter.second.timeStamp;
			shaderCode = iter.second.byteCode;

			// If we didn't find an original shader, that is OK, because it might not have been loaded yet.
			// Just skip it in that case, because the new version will be loaded when it is used.
			if (oldShader == NULL)
			{
				LogInfo("> failed to find original shader in mReloadedShaders: %ls\n", fileName);
				continue;
			}

			// Is there a good reason we are operating on a copy of the map and not the original?
			// Took me a while to work out why this wasn't working: iter.second.found = true;
			//   -DarkStarSword
			// Not a _good_ reason, but I was worried about breaking something I didn't understand, 
			// if I were to modify the original. Too many moving parts for me, no good way to test regressions.
			//   -bo3b
			G->mReloadedShaders[oldShader].found = true;

			// Check if the user has overridden the shader model:
			ShaderOverrideMap::iterator override = G->mShaderOverrideMap.find(hash);
			if (override != G->mShaderOverrideMap.end()) {
				if (override->second.model[0])
					shaderModel = override->second.model;
			}

			// If shaderModel is "bin", that means the original was loaded as a binary object, and thus shaderModel is unknown.
			// Disassemble the binary to get that string.
			if (shaderModel.compare("bin") == 0)
			{
				shaderModel = GetShaderModel(shaderCode->GetBufferPointer(), shaderCode->GetBufferSize());
				if (shaderModel.empty())
					goto err;
				G->mReloadedShaders[oldShader].shaderModel = shaderModel;
			}

			// Compile anew. If timestamp is unchanged, the code is unchanged, continue to next shader.
			ID3DBlob *pShaderBytecode = NULL;
			if (!RegenerateShader(shaderPath, fileName, shaderModel.c_str(), hash, shaderType, shaderCode, &timeStamp, headerLine, &pShaderBytecode))
				continue;

			// If we compiled but got nothing, that's a fatal error we need to report.
			if (pShaderBytecode == NULL)
				goto err;

			// Update timestamp, since we have an edited file.
			G->mReloadedShaders[oldShader].timeStamp = timeStamp;
			G->mReloadedShaders[oldShader].infoText = headerLine;

			replacement = NULL;
			// This needs to call the real CreateVertexShader, not our wrapped version
			if (shaderType.compare(L"vs") == 0)
			{
				hr = device->GetPassThroughOrigDevice1()->CreateVertexShader(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), classLinkage,
					(ID3D11VertexShader**)&replacement);
				CleanupShaderMaps(replacement);
			}
			else if (shaderType.compare(L"ps") == 0)
			{
				hr = device->GetPassThroughOrigDevice1()->CreatePixelShader(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), classLinkage,
					(ID3D11PixelShader**)&replacement);
				CleanupShaderMaps(replacement);
			}
			else if (shaderType.compare(L"cs") == 0)
			{
				hr = device->GetPassThroughOrigDevice1()->CreateComputeShader(pShaderBytecode->GetBufferPointer(),
					pShaderBytecode->GetBufferSize(), classLinkage, (ID3D11ComputeShader**)&replacement);
				CleanupShaderMaps(replacement);
			}
			else if (shaderType.compare(L"gs") == 0)
			{
				hr = device->GetPassThroughOrigDevice1()->CreateGeometryShader(pShaderBytecode->GetBufferPointer(),
					pShaderBytecode->GetBufferSize(), classLinkage, (ID3D11GeometryShader**)&replacement);
				CleanupShaderMaps(replacement);
			}
			else if (shaderType.compare(L"hs") == 0)
			{
				hr = device->GetPassThroughOrigDevice1()->CreateHullShader(pShaderBytecode->GetBufferPointer(),
					pShaderBytecode->GetBufferSize(), classLinkage, (ID3D11HullShader**)&replacement);
				CleanupShaderMaps(replacement);
			}
			else if (shaderType.compare(L"ds") == 0)
			{
				hr = device->GetPassThroughOrigDevice1()->CreateDomainShader(pShaderBytecode->GetBufferPointer(),
					pShaderBytecode->GetBufferSize(), classLinkage, (ID3D11DomainShader**)&replacement);
				CleanupShaderMaps(replacement);
			}
			if (FAILED(hr))
				goto err;


			// If we have an older reloaded shader, let's release it to avoid a memory leak.  This only happens after 1st reload.
			// New shader is loaded on GPU and ready to be used as override in VSSetShader or PSSetShader
			if (G->mReloadedShaders[oldShader].replacement != NULL)
				G->mReloadedShaders[oldShader].replacement->Release();
			G->mReloadedShaders[oldShader].replacement = replacement;

			// We do *not* replace the byteCode in the ReloadedShaders map,
			// since that is used in future CopyToFixes and ShaderRegex which
			// needs the original bytecode - this was the cause of our duplicate
			// StereoParams bug.

			// Any shaders that we load from disk are no longer
			// candidates for auto patching:
			G->mReloadedShaders[oldShader].deferred_replacement_candidate = false;

			LogInfo("> successfully reloaded shader: %ls\n", fileName);
		}
	}	// for every registered shader in mReloadedShaders 

out:
	LeaveCriticalSection(&G->mCriticalSection);

	return rc;
err:
	rc = false;
	goto out;
}



// When a shader is marked by the user, we want to automatically move it to the ShaderFixes folder
// The universal way to do this is to keep the shaderByteCode around, and when mark happens, use that as
// the replacement and build code to match.  This handles all the variants of preload, cache, hlsl 
// or not, and allows creating new files on a first run.  Should be handy.

static void CopyToFixes(UINT64 hash, HackerDevice *device)
{
	bool success = false;
	string asmText;
	string decompiled;

	// Clears any notices currently displayed on the overlay. This ensures
	// that any notices that haven't timed out yet (e.g. from a previous
	// failed dump attempt) are removed so that the only messages
	// displayed will be relevant to the current dump attempt.
	ClearNotices();

	// The key of the map is the actual shader, we thus need to do a linear search to find our marked hash.
	for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> iter in G->mReloadedShaders)
	{
		if (iter.second.hash == hash)
		{
			// Whether we succeed or fail on decompile, let's now make a screen shot of the backbuffer
			// as a good way to remember what the HLSL affects. This will be with it disabled in the picture.
			if (G->mark_snapshot == 1)
				SimpleScreenShot(device, hash, iter.second.shaderType);
			else if (G->mark_snapshot == 2)
				StereoScreenShot(device, hash, iter.second.shaderType);

			asmText = BinaryToAsmText(iter.second.byteCode->GetBufferPointer(), iter.second.byteCode->GetBufferSize());
			if (asmText.empty())
				break;

			// Disassembly file is written, now decompile the current byte code into HLSL.
			decompiled = Decompile(iter.second.byteCode, asmText);
			if (decompiled.empty())
				break;

			// Save the decompiled text, and ASM text into the .txt source file.
			if (!WriteHLSL(decompiled, asmText, hash, iter.second.shaderType))
				break;


			// Lastly, reload the shader generated, to check for decompile errors, set it as the active 
			// shader code, in case there are visual errors, and make it the match the code in the file.
			wchar_t fileName[MAX_PATH];

			swprintf_s(fileName, MAX_PATH, L"%016llx-%ls_replace.txt", hash, iter.second.shaderType.c_str());
			if (!ReloadShader(G->SHADER_PATH, fileName, device))
				break;

			// There can be more than one in the map with the same hash, but we only need a single copy to
			// make the hlsl file output, so exit with success.
			success = true;
			break;
		}
	}

	if (success)
	{
		LogOverlay(LOG_INFO, "> successfully copied Marked shader to ShaderFixes\n");
	}
	else
	{
		LogOverlay(LOG_WARNING, "> FAILED to copy Marked shader to ShaderFixes\n");
		BeepFailure();
	}
}

static void TakeScreenShot(HackerDevice *wrapped, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	LogInfo("> capturing screenshot\n");

	if (wrapped->mStereoHandle)
	{
		NvAPI_Status err;
		err = NvAPI_Stereo_CapturePngImage(wrapped->mStereoHandle);
		if (err != NVAPI_OK)
		{
			LogOverlay(LOG_WARNING, "> screenshot failed, error:%d\n", err);
		}
	}
}

// If a shader no longer exists in ShaderFixes, point it back to the original
// shader so that the replaced shaders are consistent with those in
// ShaderFixes. Especially useful if the decompiler creates a rendering issue
// in a shader we actually don't need so we don't need to restart the game.
static void RevertMissingShaders()
{
	ID3D11DeviceChild* replacement = NULL;
	ShaderReloadMap::iterator i;

	for (i = G->mReloadedShaders.begin(); i != G->mReloadedShaders.end(); i++) {
		if (i->second.found)
			continue;

		ShaderReplacementMap::iterator j = G->mOriginalShaders.find(i->first);
		if (j == G->mOriginalShaders.end())
			continue;
		replacement = j->second;

		if ((i->second.replacement == NULL && i->first == replacement)
			|| replacement == i->second.replacement) {
			continue;
		}

		LogInfo("Reverting %016llx not found in ShaderFixes\n", i->second.hash);

		if (i->second.replacement)
			i->second.replacement->Release();

		replacement->AddRef();
		i->second.replacement = replacement;
		i->second.timeStamp = { 0 };
		i->second.infoText.clear();

		// Any shaders that we revert become candidates for auto
		// patching. Elsewhere, when reloading the config we also clear
		// the processed flag so that any updated patterns in the ini
		// will be [re]applied:
		i->second.deferred_replacement_candidate = true;
	}
}

// Now that we are adding ASM files to the mix, we need to decide who gets precedence.
// Given that an ASM file is possibly being used as the more sure approach for a given
// file, it makes sense to give ASM files precedence, if both files are there.
// Using the '*' for file match will allow anything with _replace or with no tail to
// be found.  Since these will use name order, the no tail asm file will come after
// the hlsl file and replace it.  This dual file scenario is expected to be rare, so
// not doing anything heroic here to avoid that double load.

static void ReloadFixes(HackerDevice *device, void *private_data)
{
	LogInfo("> reloading *_replace.txt fixes from ShaderFixes\n");

	if (G->SHADER_PATH[0])
	{
		bool success = true;
		WIN32_FIND_DATA findFileData;
		wchar_t fileName[MAX_PATH];

		// Clears any notices currently displayed on the overlay. This ensures
		// that any notices that haven't timed out yet (e.g. from a previous
		// failed reload attempt) are removed so that the only messages
		// displayed will be relevant to the current reload attempt.
		//
		// The config reload is separate and will also attempt to clear old
		// notices - ClearNotices() itself will ensure that only the first one
		// of these actually takes effect in the current frame.
		ClearNotices();

		for (ShaderReloadMap::iterator iter = G->mReloadedShaders.begin(); iter != G->mReloadedShaders.end(); iter++)
			iter->second.found = false;

		// Strict file name format, to allow renaming out of the way. 
		// "00aa7fa12bbf66b3-ps_replace.txt" or "00aa7fa12bbf66b3-vs.txt"
		// Will still blow up if the first characters are not hex.
		swprintf_s(fileName, MAX_PATH, L"%ls\\????????????????-??*.txt", G->SHADER_PATH);
		HANDLE hFind = FindFirstFile(fileName, &findFileData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do {
				success = ReloadShader(G->SHADER_PATH, findFileData.cFileName, device) && success;
			} while (FindNextFile(hFind, &findFileData));
			FindClose(hFind);
		}

		// Any shaders in the map not visited, we want to revert back
		// to original. We do this even if a shader failed, because we
		// should still revert other shaders.
		RevertMissingShaders();

		if (success)
		{
			LogOverlay(LOG_INFO, "> successfully reloaded shaders from ShaderFixes\n");
		}
		else
		{
			LogOverlay(LOG_WARNING, "> FAILED to reload shaders from ShaderFixes\n");
			BeepFailure();
		}
	}
}

static void DisableFix(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	LogInfo("show_original pressed - switching to original shaders\n");
	G->fix_enabled = false;
}

static void EnableFix(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	LogInfo("show_original released - switching to replaced shaders\n");
	G->fix_enabled = true;
}

static void AnalyseFrame(HackerDevice *device, void *private_data)
{
	wchar_t path[MAX_PATH], subdir[MAX_PATH];
	time_t ltime;
	struct tm tm;

	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	time(&ltime);
	_localtime64_s(&tm, &ltime);
	wcsftime(subdir, MAX_PATH, L"FrameAnalysis-%Y-%m-%d-%H%M%S", &tm);

	if (!GetModuleFileName(0, path, MAX_PATH))
		return;
	wcsrchr(path, L'\\')[1] = 0;
	wcscat_s(path, MAX_PATH, subdir);

	LogInfoW(L"Frame analysis directory: %s\n", path);

	// Bail if the analysis directory already exists or can't be created.
	// This currently limits us to one / second, but that's probably
	// enough. We can always increase the granuality if needed.
	if (!CreateDirectoryEnsuringAccess(path)) {
		LogInfoW(L"Error creating frame analysis directory: %i\n", GetLastError());
		return;
	}

	wcscpy(G->ANALYSIS_PATH, path);

	G->cur_analyse_options = G->def_analyse_options;
	G->frame_analysis_seen_rts.clear();
	G->analyse_frame_no = 1;
	G->analyse_frame = true;
}

static void AnalyseFrameStop(HackerDevice *device, void *private_data)
{
	// One of two places we can stop the frame analysis - the other is in
	// the present call. We use this one when analyse_options=hold.
	// We don't allow hold to be changed mid-frame due to potential
	// for filename conflicts, so use def_analyse_options:
	if (G->analyse_frame && (G->def_analyse_options & FrameAnalysisOptions::HOLD)) {
		G->analyse_frame = false;
		if (G->DumpUsage) {
			EnterCriticalSection(&G->mCriticalSection);
				DumpUsage(G->ANALYSIS_PATH);
			LeaveCriticalSection(&G->mCriticalSection);
		}
	}
}

static void DisableDeferred(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	LogInfo("Disabling execution of deferred command lists\n");
	G->deferred_contexts_enabled = false;
}

static void EnableDeferred(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	LogInfo("Enabling execution of deferred command lists\n");
	G->deferred_contexts_enabled = true;
}


template <typename ItemType>
static void HuntNext(char *type, std::set<ItemType> *visited,
		ItemType *selected, int *selectedPos)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	EnterCriticalSection(&G->mCriticalSection);
	{
		std::set<ItemType>::iterator loc = visited->find(*selected);
		std::set<ItemType>::iterator end = visited->end();
		bool found = (loc != end);
		int size = (int) visited->size();

		if (size == 0)
			goto out;

		if (found) {
			loc++;
			if (loc != end) {
				(*selectedPos)++;
				*selected = *loc;
			} else {
				*selectedPos = 0;
				*selected = *visited->begin();
			}
			LogInfo("> traversing to next %s #%d. Number of %ss in frame: %d\n",
					type, *selectedPos, type, size);
		} else {
			*selectedPos = 0;
			*selected = *visited->begin();
			LogInfo("> starting at %s #%d. Number of %ss in frame: %d\n",
					type, *selectedPos, type, size);
		}
	}
out:
	LeaveCriticalSection(&G->mCriticalSection);
}

static void NextIndexBuffer(HackerDevice *device, void *private_data)
{
	HuntNext<uint32_t>("index buffer", &G->mVisitedIndexBuffers, &G->mSelectedIndexBuffer, &G->mSelectedIndexBufferPos);

	EnterCriticalSection(&G->mCriticalSection);
	G->mSelectedIndexBuffer_PixelShader.clear();
	G->mSelectedIndexBuffer_VertexShader.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}
static void NextPixelShader(HackerDevice *device, void *private_data)
{
	HuntNext<UINT64>("pixel shader", &G->mVisitedPixelShaders, &G->mSelectedPixelShader, &G->mSelectedPixelShaderPos);

	EnterCriticalSection(&G->mCriticalSection);
	G->mSelectedPixelShader_IndexBuffer.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}
static void NextVertexShader(HackerDevice *device, void *private_data)
{
	HuntNext<UINT64>("vertex shader", &G->mVisitedVertexShaders, &G->mSelectedVertexShader, &G->mSelectedVertexShaderPos);

	EnterCriticalSection(&G->mCriticalSection);
	G->mSelectedVertexShader_IndexBuffer.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}
static void NextComputeShader(HackerDevice *device, void *private_data)
{
	HuntNext<UINT64>("compute shader", &G->mVisitedComputeShaders, &G->mSelectedComputeShader, &G->mSelectedComputeShaderPos);
}
static void NextGeometryShader(HackerDevice *device, void *private_data)
{
	HuntNext<UINT64>("geometry shader", &G->mVisitedGeometryShaders, &G->mSelectedGeometryShader, &G->mSelectedGeometryShaderPos);
}
static void NextDomainShader(HackerDevice *device, void *private_data)
{
	HuntNext<UINT64>("domain shader", &G->mVisitedDomainShaders, &G->mSelectedDomainShader, &G->mSelectedDomainShaderPos);
}
static void NextHullShader(HackerDevice *device, void *private_data)
{
	HuntNext<UINT64>("hull shader", &G->mVisitedHullShaders, &G->mSelectedHullShader, &G->mSelectedHullShaderPos);
}
static void NextRenderTarget(HackerDevice *device, void *private_data)
{
	HuntNext<ID3D11Resource *>("render target", &G->mVisitedRenderTargets, &G->mSelectedRenderTarget, &G->mSelectedRenderTargetPos);
}

template <typename ItemType>
static void HuntPrev(char *type, std::set<ItemType> *visited,
		ItemType *selected, int *selectedPos)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	EnterCriticalSection(&G->mCriticalSection);
	{
		std::set<ItemType>::iterator loc = visited->find(*selected);
		std::set<ItemType>::iterator end = visited->end();
		std::set<ItemType>::iterator front = visited->begin();
		bool found = (loc != end);
		int size = (int) visited->size();

		if (size == 0)
			goto out;

		if (found) {
			if (loc != front) {
				(*selectedPos)--;
				loc--;
				*selected = *loc;
			} else {
				*selectedPos = size - 1;
				*selected = *std::prev(end);
			}
			LogInfo("> traversing to previous %s shader #%d. Number of %s shaders in frame: %d\n",
					type, *selectedPos, type, size);
		} else {
			*selectedPos = size - 1;
			*selected = *std::prev(end);
			LogInfo("> starting at %s shader #%d. Number of %s shaders in frame: %d\n",
					type, *selectedPos, type, size);
		}
	}
out:
	LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevIndexBuffer(HackerDevice *device, void *private_data)
{
	HuntPrev<uint32_t>("index buffer", &G->mVisitedIndexBuffers, &G->mSelectedIndexBuffer, &G->mSelectedIndexBufferPos);

	EnterCriticalSection(&G->mCriticalSection);
	G->mSelectedIndexBuffer_PixelShader.clear();
	G->mSelectedIndexBuffer_VertexShader.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}
static void PrevPixelShader(HackerDevice *device, void *private_data)
{
	HuntPrev<UINT64>("pixel shader", &G->mVisitedPixelShaders, &G->mSelectedPixelShader, &G->mSelectedPixelShaderPos);

	EnterCriticalSection(&G->mCriticalSection);
	G->mSelectedPixelShader_IndexBuffer.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}
static void PrevVertexShader(HackerDevice *device, void *private_data)
{
	HuntPrev<UINT64>("vertex shader", &G->mVisitedVertexShaders, &G->mSelectedVertexShader, &G->mSelectedVertexShaderPos);

	EnterCriticalSection(&G->mCriticalSection);
	G->mSelectedVertexShader_IndexBuffer.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}
static void PrevComputeShader(HackerDevice *device, void *private_data)
{
	HuntPrev<UINT64>("compute shader", &G->mVisitedComputeShaders, &G->mSelectedComputeShader, &G->mSelectedComputeShaderPos);
}
static void PrevGeometryShader(HackerDevice *device, void *private_data)
{
	HuntPrev<UINT64>("geometry shader", &G->mVisitedGeometryShaders, &G->mSelectedGeometryShader, &G->mSelectedGeometryShaderPos);
}
static void PrevDomainShader(HackerDevice *device, void *private_data)
{
	HuntPrev<UINT64>("domain shader", &G->mVisitedDomainShaders, &G->mSelectedDomainShader, &G->mSelectedDomainShaderPos);
}
static void PrevHullShader(HackerDevice *device, void *private_data)
{
	HuntPrev<UINT64>("hull shader", &G->mVisitedHullShaders, &G->mSelectedHullShader, &G->mSelectedHullShaderPos);
}
static void PrevRenderTarget(HackerDevice *device, void *private_data)
{
	HuntPrev<ID3D11Resource *>("render target", &G->mVisitedRenderTargets, &G->mSelectedRenderTarget, &G->mSelectedRenderTargetPos);
}



static void MarkIndexBuffer(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	EnterCriticalSection(&G->mCriticalSection);

	LogInfo(">>>> Index buffer marked: index buffer hash = %08x\n", G->mSelectedIndexBuffer);
	for (std::set<UINT64>::iterator i = G->mSelectedIndexBuffer_PixelShader.begin(); i != G->mSelectedIndexBuffer_PixelShader.end(); ++i)
		LogInfo("     visited pixel shader hash = %016I64x\n", *i);
	for (std::set<UINT64>::iterator i = G->mSelectedIndexBuffer_VertexShader.begin(); i != G->mSelectedIndexBuffer_VertexShader.end(); ++i)
		LogInfo("     visited vertex shader hash = %016I64x\n", *i);

	if (G->DumpUsage)
		DumpUsage(NULL);

	LeaveCriticalSection(&G->mCriticalSection);
}

static bool MarkShaderBegin(char *type, UINT64 selected)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return false;

	EnterCriticalSection(&G->mCriticalSection);

	LogInfo(">>>> %s marked: %s hash = %016I64x\n", type, type, selected);

	return true;
}
static void MarkShaderEnd(HackerDevice *device, char *type, UINT64 selected)
{
	CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(selected);
	if (i != G->mCompiledShaderMap.end())
		LogInfo("       %s was compiled from source code %s\n", type, i->second.c_str());

	// Copy marked shader to ShaderFixes
	CopyToFixes(selected, device);

	if (G->DumpUsage)
		DumpUsage(NULL);

	LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkPixelShader(HackerDevice *device, void *private_data)
{
	if (!MarkShaderBegin("pixel shader", G->mSelectedPixelShader))
		return;

	for (std::set<uint32_t>::iterator i = G->mSelectedPixelShader_IndexBuffer.begin(); i != G->mSelectedPixelShader_IndexBuffer.end(); ++i)
		LogInfo("     visited index buffer hash = %08x\n", *i);
	for (std::set<UINT64>::iterator i = G->mPixelShaderInfo[G->mSelectedPixelShader].PeerShaders.begin(); i != G->mPixelShaderInfo[G->mSelectedPixelShader].PeerShaders.end(); ++i)
		LogInfo("     visited peer shader hash = %016I64x\n", *i);

	MarkShaderEnd(device, "pixel shader", G->mSelectedPixelShader);
}

static void MarkVertexShader(HackerDevice *device, void *private_data)
{
	if (!MarkShaderBegin("vertex shader", G->mSelectedVertexShader))
		return;

	for (std::set<uint32_t>::iterator i = G->mSelectedVertexShader_IndexBuffer.begin(); i != G->mSelectedVertexShader_IndexBuffer.end(); ++i)
		LogInfo("     visited index buffer hash = %08lx\n", *i);
	for (std::set<UINT64>::iterator i = G->mVertexShaderInfo[G->mSelectedVertexShader].PeerShaders.begin(); i != G->mVertexShaderInfo[G->mSelectedVertexShader].PeerShaders.end(); ++i)
		LogInfo("     visited peer shader hash = %016I64x\n", *i);

	MarkShaderEnd(device, "vertex shader", G->mSelectedVertexShader);
}

static void MarkComputeShader(HackerDevice *device, void *private_data)
{
	if (!MarkShaderBegin("compute shader", G->mSelectedComputeShader))
		return;
	MarkShaderEnd(device, "compute shader", G->mSelectedComputeShader);
}

static void MarkGeometryShader(HackerDevice *device, void *private_data)
{
	if (!MarkShaderBegin("geometry shader", G->mSelectedGeometryShader))
		return;

	for (std::set<UINT64>::iterator i = G->mGeometryShaderInfo[G->mSelectedGeometryShader].PeerShaders.begin(); i != G->mGeometryShaderInfo[G->mSelectedGeometryShader].PeerShaders.end(); ++i)
		LogInfo("     visited peer shader hash = %016I64x\n", *i);

	MarkShaderEnd(device, "geometry shader", G->mSelectedGeometryShader);
}

static void MarkDomainShader(HackerDevice *device, void *private_data)
{
	if (!MarkShaderBegin("domain shader", G->mSelectedDomainShader))
		return;

	for (std::set<UINT64>::iterator i = G->mDomainShaderInfo[G->mSelectedDomainShader].PeerShaders.begin(); i != G->mDomainShaderInfo[G->mSelectedDomainShader].PeerShaders.end(); ++i)
		LogInfo("     visited peer shader hash = %016I64x\n", *i);

	MarkShaderEnd(device, "domain shader", G->mSelectedDomainShader);
}

static void MarkHullShader(HackerDevice *device, void *private_data)
{
	if (!MarkShaderBegin("hull shader", G->mSelectedHullShader))
		return;

	for (std::set<UINT64>::iterator i = G->mHullShaderInfo[G->mSelectedHullShader].PeerShaders.begin(); i != G->mHullShaderInfo[G->mSelectedHullShader].PeerShaders.end(); ++i)
		LogInfo("     visited peer shader hash = %016I64x\n", *i);

	MarkShaderEnd(device, "hull shader", G->mSelectedHullShader);
}

static void LogRenderTarget(ID3D11Resource *target, char *log_prefix)
{
	char buf[256];

	if (!target || target == (ID3D11Resource *)-1)
	{
		LogInfo("No render target selected for marking\n");
		return;
	}

	uint32_t hash = G->mResources[target].hash;
	uint32_t orig_hash = G->mResources[target].orig_hash;
	struct ResourceHashInfo &info = G->mResourceInfo[orig_hash];
	StrResourceDesc(buf, 256, info);
	LogInfo("%srender target handle = %p, hash = %08lx, orig_hash = %08lx, %s\n",
		log_prefix, target, hash, orig_hash, buf);
}

static void MarkRenderTarget(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	EnterCriticalSection(&G->mCriticalSection);

	LogRenderTarget(G->mSelectedRenderTarget, ">>>> Render target marked: ");
	for (std::set<ID3D11Resource *>::iterator i = G->mSelectedRenderTargetSnapshotList.begin(); i != G->mSelectedRenderTargetSnapshotList.end(); ++i)
		LogRenderTarget(*i, "       ");

	if (G->DumpUsage)
		DumpUsage(NULL);

	LeaveCriticalSection(&G->mCriticalSection);
}


static void TuneUp(HackerDevice *device, void *private_data)
{
	intptr_t index = (intptr_t)private_data;

	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	G->gTuneValue[index] += G->gTuneStep;
	LogInfo("> Value %Ii tuned to %f\n", index + 1, G->gTuneValue[index]);
}

static void TuneDown(HackerDevice *device, void *private_data)
{
	intptr_t index = (intptr_t)private_data;

	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	G->gTuneValue[index] -= G->gTuneStep;
	LogInfo("> Value %Ii tuned to %f\n", index + 1, G->gTuneValue[index]);
}


// Start with a fresh set of shaders in the scene - either called explicitly
// via keypress, or after no hunting for 1 minute (see comment in RunFrameActions)
// Caller must have taken G->mCriticalSection (if enabled)
void TimeoutHuntingBuffers()
{
	G->mVisitedIndexBuffers.clear();
	G->mVisitedVertexShaders.clear();
	G->mVisitedPixelShaders.clear();
	G->mVisitedComputeShaders.clear();
	G->mVisitedGeometryShaders.clear();
	G->mVisitedDomainShaders.clear();
	G->mVisitedHullShaders.clear();
	G->mVisitedRenderTargets.clear();
}

// User has requested all shaders be re-enabled
static void DoneHunting(HackerDevice *device, void *private_data)
{
	if (G->hunting != HUNTING_MODE_ENABLED)
		return;

	EnterCriticalSection(&G->mCriticalSection);

	TimeoutHuntingBuffers();

	G->mSelectedPixelShader = -1;
	G->mSelectedPixelShaderPos = -1;
	G->mSelectedVertexShader = -1;
	G->mSelectedVertexShaderPos = -1;
	G->mSelectedComputeShader = -1;
	G->mSelectedComputeShaderPos = -1;
	G->mSelectedGeometryShader = -1;
	G->mSelectedGeometryShaderPos = -1;
	G->mSelectedDomainShader = -1;
	G->mSelectedDomainShaderPos = -1;
	G->mSelectedHullShader = -1;
	G->mSelectedHullShaderPos = -1;

	G->mSelectedRenderTargetPos = -1;
	G->mSelectedRenderTarget = ((ID3D11Resource *)-1);
	G->mSelectedIndexBuffer = -1;
	G->mSelectedIndexBufferPos = -1;

	G->mSelectedPixelShader_IndexBuffer.clear();
	G->mSelectedVertexShader_IndexBuffer.clear();
	G->mSelectedIndexBuffer_PixelShader.clear();
	G->mSelectedIndexBuffer_VertexShader.clear();

	LeaveCriticalSection(&G->mCriticalSection);
}

static void ToggleHunting(HackerDevice *device, void *private_data)
{
	if (G->hunting == HUNTING_MODE_ENABLED)
		G->hunting = HUNTING_MODE_SOFT_DISABLED;
	else
		G->hunting = HUNTING_MODE_ENABLED;
	LogInfo("> Hunting toggled to %d\n", G->hunting);
}

void RegisterHuntingKeyBindings()
{
	intptr_t i;
	wchar_t buf[MAX_PATH];
	int repeat = 8, noRepeat = 0;

	// reload_config is registered even if not hunting - this allows us to
	// turn on hunting in the ini dynamically without having to relaunch
	// the game. This can be useful in games that receive a significant
	// performance hit with hunting on, or where a broken effect is
	// discovered while playing normally where it may not be easy/fast to
	// find the effect again later.
	G->config_reloadable = RegisterIniKeyBinding(L"Hunting", L"reload_config", FlagConfigReload, NULL, noRepeat, NULL);

	// Don't register hunting keys when hard disabled. In this case the
	// only way to turn hunting on is to edit the ini file and reload it.
	if (G->hunting == HUNTING_MODE_DISABLED)
		return;

	// Let's also allow an easy toggle of hunting itself, for speed and playability.
	RegisterIniKeyBinding(L"Hunting", L"toggle_hunting", ToggleHunting, NULL, noRepeat, NULL);

	repeat = GetIniInt(L"Hunting", L"repeat_rate", repeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_pixelshader", NextPixelShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_pixelshader", PrevPixelShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_pixelshader", MarkPixelShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"take_screenshot", TakeScreenShot, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_indexbuffer", NextIndexBuffer, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_indexbuffer", PrevIndexBuffer, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_indexbuffer", MarkIndexBuffer, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_vertexshader", NextVertexShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_vertexshader", PrevVertexShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_vertexshader", MarkVertexShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_computeshader", NextComputeShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_computeshader", PrevComputeShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_computeshader", MarkComputeShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_geometryshader", NextGeometryShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_geometryshader", PrevGeometryShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_geometryshader", MarkGeometryShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_domainshader", NextDomainShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_domainshader", PrevDomainShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_domainshader", MarkDomainShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_hullshader", NextHullShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_hullshader", PrevHullShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_hullshader", MarkHullShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_rendertarget", NextRenderTarget, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_rendertarget", PrevRenderTarget, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_rendertarget", MarkRenderTarget, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"done_hunting", DoneHunting, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"reload_fixes", ReloadFixes, NULL, noRepeat, NULL);

	G->show_original_enabled = RegisterIniKeyBinding(L"Hunting", L"show_original", DisableFix, EnableFix, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"analyse_frame", AnalyseFrame, AnalyseFrameStop, noRepeat, NULL);
	if (GetIniStringAndLog(L"Hunting", L"analyse_options", 0, buf, MAX_PATH)) {
		G->def_analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
			(FrameAnalysisOptionNames, buf, NULL);
	} else
		G->def_analyse_options = FrameAnalysisOptions::INVALID;

	// Quick hacks to see if DX11 features that we only have limited support for are responsible for anything important:
	RegisterIniKeyBinding(L"Hunting", L"kill_deferred", DisableDeferred, EnableDeferred, noRepeat, NULL);

	G->ENABLE_TUNE = GetIniBool(L"Hunting", L"tune_enable", false, NULL);
	G->gTuneStep = GetIniFloat(L"Hunting", L"tune_step", 1.0f, NULL);

	for (i = 0; i < 4; i++) {
		_snwprintf(buf, 16, L"tune%Ii_up", i + 1);
		RegisterIniKeyBinding(L"Hunting", buf, TuneUp, NULL, repeat, (void*)i);

		_snwprintf(buf, 16, L"tune%Ii_down", i + 1);
		RegisterIniKeyBinding(L"Hunting", buf, TuneDown, NULL, repeat, (void*)i);
	}

	G->verbose_overlay = GetIniBool(L"Hunting", L"verbose_overlay", false, NULL);
}
