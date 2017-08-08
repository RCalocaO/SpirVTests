// SpirVTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "C:\\VulkanSDK\\1.0.51.0\\Include\\vulkan\\spirv.h"
#include <vector>
#include <stdio.h>
#include <assert.h>
#include <string>


const char* SPVFile = "C:\\Users\\Rolando Caloca\\Documents\\GitHub\\SpirVTests\\SimpleElementPS\\Output.spv";

std::vector<uint32_t> Load(const char* Filename)
{
	std::vector<uint32_t> Data;
	FILE* File = fopen(Filename, "rb");
	if (File)
	{
		fseek(File, 0, SEEK_END);
		auto Size = ftell(File);
		Data.resize(Size);
		fseek(File, 0, SEEK_SET);
		fread(&Data[0], 1, Size, File);
		fclose(File);
	}

	return Data;
}


#define Verify(b) if (!(b)) { __debugbreak(); }

uint32_t ReadHeader(uint32_t*& Ptr)
{
	Verify(*Ptr++ == SpvMagicNumber);
	uint32_t Version = *Ptr++;
	uint32_t Generator = *Ptr++;
	uint32_t Bound = *Ptr++;
	Verify(*Ptr++ == 0);
	return Bound;
}

void ProcessExecModel(SpvExecutionModel Model)
{
	switch (Model)
	{
	case SpvExecutionModelFragment:
		break;
	default:
		Verify(false);
		break;
	}
}

std::string ReadLiteralString(uint32_t*& Ptr)
{
	std::string S = (char*)Ptr;
	Ptr += (S.length() + 3 + 1) / 4;
	return S;
}

struct FScope
{
	uint32_t* SrcPtr;
	uint32_t*& Ptr;
	uint32_t WordCount;

	FScope(uint32_t*& OriginalPtr, uint32_t InWordCount)
		: SrcPtr(OriginalPtr)
		, Ptr(OriginalPtr)
		, WordCount(InWordCount)
	{
	}

	~FScope()
	{
		Verify(SrcPtr + WordCount == Ptr);
	}
};

void ReadDecoration(uint32_t*& Ptr, SpvDecoration Decoration)
{
	switch (Decoration)
	{
	case SpvDecorationArrayStride:
	{
		uint32_t ArrayStride = *Ptr++;
	}
		break;
	case SpvDecorationOffset:
	{
		uint32_t ByteOffset = *Ptr++;
	}
	break;
	case SpvDecorationBlock:
	case SpvDecorationBufferBlock:
	case SpvDecorationRowMajor:
		break;
	case SpvDecorationDescriptorSet:
	{
		uint32_t DescriptorSet = *Ptr++;
	}
	break;
	case SpvDecorationBinding:
	{
		uint32_t Binding = *Ptr++;
	}
	break;
	case SpvDecorationBuiltIn:
	{
		SpvBuiltIn Builtin = (SpvBuiltIn)*Ptr++;
	}
	break;
	case SpvDecorationLocation:
	{
		uint32_t Location = *Ptr++;
	}
	break;
	default:
		Verify(false);
		break;
	}
}

int main()
{
	auto Data = Load(SPVFile);
	uint32_t* Ptr = &Data[0];
	uint32_t* End = Ptr + Data.size();
	uint32_t Bound = ReadHeader(Ptr);

	while (Ptr < End)
	{
		bool bAutoIncrease = true;
		Verify(*Ptr);
		uint32_t WordCount = (*Ptr >> 16) & 65535;
		SpvOp OpCode = (SpvOp)(*Ptr & 65535);

		switch (OpCode)
		{
		case SpvOpCapability:
		case SpvOpExtInstImport:
		case SpvOpMemoryModel:
			break;
		case SpvOpEntryPoint:
		{
			FScope Scope(Ptr, WordCount);
			Ptr++;
			SpvExecutionModel ExecModel = (SpvExecutionModel)*Ptr++;
			ProcessExecModel(ExecModel);
			uint32_t EntryPoint = *Ptr++;
			auto EntryName = ReadLiteralString(Ptr);
			int32_t NumInterfaces = (Scope.SrcPtr + WordCount) - Ptr;
			Verify(NumInterfaces >= 0);
			for (int32_t Index = 0; Index < NumInterfaces; ++Index)
			{
				uint32_t Interface = *Ptr++;
			}
			bAutoIncrease = false;
		}
			break;
		case SpvOpExecutionMode:
		case SpvOpSource:
		case SpvOpSourceExtension:
			break;
		case SpvOpName:
		{
			FScope Scope(Ptr, WordCount);
			Ptr++;
			uint32_t Target = *Ptr++;
			auto Name = ReadLiteralString(Ptr);
			bAutoIncrease = false;
		}
			break;
		case SpvOpMemberName:
		{
			FScope Scope(Ptr, WordCount);
			Ptr++;
			uint32_t Type = *Ptr++;
			auto Member = *Ptr++;
			auto Name = ReadLiteralString(Ptr);
			bAutoIncrease = false;
		}
			break;
		case SpvOpDecorate:
		{
			FScope Scope(Ptr, WordCount);
			*Ptr++;
			uint32_t Target = *Ptr++;
			SpvDecoration Decoration = (SpvDecoration)*Ptr++;
			ReadDecoration(Ptr, Decoration);
			bAutoIncrease = false;
		}
			break;
		case SpvOpMemberDecorate:
		{
			FScope Scope(Ptr, WordCount);
			*Ptr++;
			uint32_t StructureType = *Ptr++;
			uint32_t Member = *Ptr++;
			SpvDecoration Decoration = (SpvDecoration)*Ptr++;
			ReadDecoration(Ptr, Decoration);
			bAutoIncrease = false;
		}
			break;
		case SpvOpTypeVoid:
			break;
		default:
			Verify(false);
		}

		if (bAutoIncrease)
		{
			Ptr += WordCount;
		}
	}

	Verify(Ptr == End);
	return 0;
}

