// SpirVTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "C:\\VulkanSDK\\1.0.51.0\\Include\\vulkan\\spirv.h"
#include <vector>
#include <stack>
#include <stdio.h>
#include <assert.h>
#include <string>


const char* SPVFile = ".\\SimpleElementPS\\Output.spv";

std::vector<uint32_t> Load(const char* Filename)
{
	std::vector<uint32_t> Data;
	FILE* File;
	fopen_s(&File, Filename, "rb");
	if (File)
	{
		fseek(File, 0, SEEK_END);
		auto Size = ftell(File);
		Data.resize((Size + 3) / 4);
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

struct FSpirVParser
{
	std::vector<uint32_t> Data;
	uint32_t* Ptr = nullptr;
	uint32_t* End = nullptr;
	uint32_t* PrevPtr = nullptr;
	SpvOp PrevOpCode = (SpvOp)-1;

	bool Init(const char* File)
	{
		Data = Load(SPVFile);
		if (Data.empty())
		{
			return false;
		}

		Ptr = &Data[0];
		End = Ptr + Data.size();

		uint32_t Bound = ReadHeader(Ptr);

		return true;
	}

	void Process()
	{
		while (Ptr < End)
		{
			bool bAutoIncrease = true;
			Verify(*Ptr);
			uint32_t* SrcPtr = Ptr;
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
				int32_t NumInterfaces = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
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
			case SpvOpTypeFunction:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t ReturnType = *Ptr++;
				int32_t NumParams = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumParams; ++Index)
				{
					uint32_t ParamType = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeInt:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Width = *Ptr++;
				uint32_t Signedness = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeFloat:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Width = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeBool:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypePointer:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				SpvStorageClass StorageClass = (SpvStorageClass)*Ptr++;
				uint32_t Type = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeVector:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t ComponentType = *Ptr++;
				uint32_t ComponentCount = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeArray:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t ElementType = *Ptr++;
				uint32_t Length = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpConstant:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				int32_t NumValues = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumValues; ++Index)
				{
					uint32_t Value = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeStruct:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				int32_t NumMembers = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumMembers; ++Index)
				{
					uint32_t MemberType = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpVariable:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				SpvStorageClass StorageClass = (SpvStorageClass)*Ptr++;
				int32_t HasInitializer = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasInitializer == 0 || HasInitializer == 1);
				if (HasInitializer == 1)
				{
					uint32_t Initializer = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeImage:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t SampledType = *Ptr++;
				SpvDim Dim = (SpvDim)*Ptr++;
				uint32_t Depth = *Ptr++;
				uint32_t Arrayed = *Ptr++;
				uint32_t MS = *Ptr++;
				uint32_t Sampled = *Ptr++;
				SpvImageFormat ImageFormat = (SpvImageFormat)*Ptr++;

				int32_t HasQualifier = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasQualifier == 0 || HasQualifier == 1);
				if (HasQualifier == 1)
				{
					SpvAccessQualifier Qualifier = (SpvAccessQualifier)*Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpConstantComposite:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				int32_t NumElements = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumElements; ++Index)
				{
					uint32_t Element = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpCompositeConstruct:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				int32_t NumElements = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumElements; ++Index)
				{
					uint32_t Element = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeSampledImage:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t ImageType = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpFunction:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t FunctionControl = /*SpvFunctionControlMask*/*Ptr++;
				uint32_t FunctionType = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpFunctionEnd:
				break;
			case SpvOpReturn:
			case SpvOpKill:
				break;
			case SpvOpReturnValue:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpLabel:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpBranch:
			{
				*Ptr++;
				uint32_t Result = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpAccessChain:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Base = *Ptr++;
				int32_t NumIndices = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumIndices; ++Index)
				{
					uint32_t ChainIndex = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpLoad:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Pointer = *Ptr++;
				int32_t HasMemoryAccess = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasMemoryAccess == 0 || HasMemoryAccess == 1);
				if (HasMemoryAccess == 1)
				{
					uint32_t MemoryAccess = /*SpvMemoryAccessMask*/*Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpStore:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t Pointer = *Ptr++;
				uint32_t Object = *Ptr++;
				int32_t HasMemoryAccess = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasMemoryAccess == 0 || HasMemoryAccess == 1);
				if (HasMemoryAccess == 1)
				{
					uint32_t MemoryAccess = /*SpvMemoryAccessMask*/*Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpTypeRuntimeArray:
			{
				Verify(0);
				/*
				*Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t ElementType = *Ptr++;
				bAutoIncrease = false;
				*/
			}
			break;
			case SpvOpINotEqual:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Op1 = *Ptr++;
				uint32_t Op2 = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpSelectionMerge:
			{
				*Ptr++;
				uint32_t MergeBlock = *Ptr++;
				uint32_t SelectionControl = /*SpvSelectionControlMask*/*Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpConvertFToS:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t FloatValue = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpFOrdEqual:
			case SpvOpFOrdNotEqual:
			case SpvOpFOrdLessThanEqual:
			case SpvOpFOrdLessThan:
			case SpvOpFOrdGreaterThan:
			case SpvOpFOrdGreaterThanEqual:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Operand1 = *Ptr++;
				uint32_t Operand2 = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpFNegate:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Operand = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpAny:
			case SpvOpAll:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Vector = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpFAdd:
			case SpvOpFDiv:
			case SpvOpFSub:
			case SpvOpFMul:
			case SpvOpFMod:
			case SpvOpDot:
			case SpvOpOuterProduct:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Operand1 = *Ptr++;
				uint32_t Operand2 = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpImage:
			{
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t SampledImage = *Ptr++;
				bAutoIncrease = false;
			}
			break;
			case SpvOpImageSampleImplicitLod:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t SampledImage = *Ptr++;
				uint32_t Coordinate = *Ptr++;
				int32_t NumOperands = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumOperands; ++Index)
				{
					uint32_t Operand = /*SpvImageOperandsMask*/*Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpBranchConditional:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t Condition = *Ptr++;
				uint32_t TrueLabel = *Ptr++;
				uint32_t FalseLabel = *Ptr++;
				int32_t NumBranchWeights = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumBranchWeights; ++Index)
				{
					uint32_t BranchWeight = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpVectorShuffle:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Vector1 = *Ptr++;
				uint32_t Vector2 = *Ptr++;
				int32_t NumComponents = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumComponents; ++Index)
				{
					uint32_t Component = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpImageFetch:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Image = *Ptr++;
				uint32_t Coordinate = *Ptr++;
				int32_t NumComponents = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumComponents; ++Index)
				{
					uint32_t ImageOperands = /*SpvImageOperandsMask*/*Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			case SpvOpCompositeExtract:
			{
				FScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t Composite = *Ptr++;
				int32_t NumComponents = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumComponents; ++Index)
				{
					int32_t Literal = *Ptr++;
				}
				bAutoIncrease = false;
			}
			break;
			default:
				Verify(false);
			}

			if (bAutoIncrease)
			{
				Ptr += WordCount;
			}

			Verify(SrcPtr + WordCount == Ptr);
			PrevOpCode = OpCode;
			PrevPtr = Ptr;
		}

		Verify(Ptr == End);
	}
};

int main()
{
	FSpirVParser Parser;
	if (!Parser.Init(SPVFile))
	{
		return 1;
	}

	Parser.Process();

	return 0;
}

