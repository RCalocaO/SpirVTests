// SpirVTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "C:\\VulkanSDK\\1.0.51.0\\Include\\vulkan\\spirv.h"
#include <vector>
#include <stack>
#include <stdio.h>
#include <assert.h>
#include <string>
#include <map>


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

struct FReaderScope
{
	uint32_t* SrcPtr;
	uint32_t*& Ptr;
	uint32_t WordCount;

	FReaderScope(uint32_t*& OriginalPtr, uint32_t InWordCount)
		: SrcPtr(OriginalPtr)
		, Ptr(OriginalPtr)
		, WordCount(InWordCount)
	{
	}

	~FReaderScope()
	{
		Verify(SrcPtr + WordCount == Ptr);
	}
};

std::vector<uint32_t> ReadDecoration(uint32_t*& Ptr, SpvDecoration Decoration)
{
	std::vector<uint32_t> Decorations;
	switch (Decoration)
	{
	case SpvDecorationBlock:
	case SpvDecorationBufferBlock:
	case SpvDecorationRowMajor:
		break;
	case SpvDecorationArrayStride:
	case SpvDecorationOffset:
	case SpvDecorationDescriptorSet:
	case SpvDecorationBinding:
	case SpvDecorationBuiltIn:
	case SpvDecorationLocation:
	{
		uint32_t ArrayStride = *Ptr++;
		Decorations.push_back(ArrayStride);
	}
	break;
	default:
		Verify(false);
		break;
	}
	return Decorations;
}

struct FEntryPoint
{
	std::string Name;
	uint32_t Id;
	std::vector<uint32_t> Interfaces;
};

struct FConstant
{
	uint32_t Type;
	std::vector<uint32_t> Values;
};

struct FConstantComposite
{
	uint32_t Type;
	std::vector<uint32_t> Constituents;
};

struct FLoad
{
	uint32_t ResultType;
	uint32_t Pointer;
	bool bHasMemoryAccess;
	uint32_t MemoryAccess;
};

struct FAccessChain
{
	uint32_t Type;
	uint32_t Base;
	std::vector<uint32_t> Indices;
};

struct FVariable
{
	uint32_t Type;
	SpvStorageClass StorageClass = SpvStorageClassMax;
	bool bInitializer = false;
	uint32_t Initializer;
};

struct FBlock
{
	uint32_t Label;
	std::vector<uint32_t> Instructions;
};

struct FFunction
{
	uint32_t ResultType;
	uint32_t FunctionControl = 0;
	uint32_t FunctionType;

	std::vector<FBlock> Blocks;
};

struct FType
{
	enum class EType
	{
		Unknown = -1,
		Void,
		Bool,
		Int,
		Float,
		Vector,
		Array,
		Struct,
		Pointer,
		Image,
		SampledImage,
		Function,
	};
	EType Type = EType::Unknown;

	struct
	{
		uint32_t ReturnType = 0;
		std::vector<uint32_t> ParameterTypes;
	} Function;

	uint32_t Width = 0;
	uint32_t Signedness = 0;

	struct
	{
		uint32_t NumComponents = 0;
		uint32_t ComponentType = 0;
	} Vector;

	struct
	{
		uint32_t Length = 0;
		uint32_t ElementType = 0;
	} Array;

	struct
	{
		std::vector<uint32_t> MemberTypes;
	} Struct;

	struct
	{
		SpvStorageClass StorageClass = SpvStorageClassMax;
		uint32_t Type;
	} Pointer;

	struct
	{
		uint32_t SampledType = 0;
		SpvDim Dim = SpvDimMax;
		uint32_t Depth = 0;
		uint32_t Arrayed = 0;
		uint32_t MS = 0;
		uint32_t Sampled = 0;
		SpvImageFormat ImageFormat = SpvImageFormatUnknown;
		SpvAccessQualifier Qualifier = SpvAccessQualifierMax;
	} Image;

	uint32_t ImageType;
};

struct FSpirVParser
{
	std::vector<uint32_t> Data;
	uint32_t* Ptr = nullptr;
	uint32_t* End = nullptr;
	uint32_t* PrevPtr = nullptr;
	SpvOp PrevOpCode = (SpvOp)-1;

	std::vector<FEntryPoint> EntryPoints;

	std::map<uint32_t, std::string> Names;

	struct FMemberName
	{
		uint32_t MemberIndex;
		std::string Name;
	};
	// Key is StructType
	std::map<uint32_t, std::vector<FMemberName>> MemberNames;

	struct FDecoration
	{
		SpvDecoration Decoration;
		std::vector<uint32_t> Literals;
	};
	std::map<uint32_t, std::vector<FDecoration>> Decorations;

	struct FMemberDecoration : public FDecoration
	{
		uint32_t MemberIndex;
	};
	// Key is StructType
	std::map<uint32_t, std::vector<FDecoration>> MemberDecorations;

	std::map<uint32_t, FType> Types;

	std::map<uint32_t, FConstant> Constants;
	std::map<uint32_t, FConstantComposite> ConstantComposites;

	std::map<uint32_t, FVariable> Variables;
	std::map<uint32_t, FFunction> Functions;
	std::map<uint32_t, FAccessChain> AccessChains;
	std::map<uint32_t, FLoad> Loads;

	uint32_t CurrentFunction = 0;

	std::vector<uint32_t> Instructions;

	std::vector<uint32_t>& GetInstructions()
	{
		if (CurrentFunction == 0)
		{
			return Instructions;
		}
		else
		{
			return GetFunction().Blocks.back().Instructions;
		}
	}

	FFunction& GetFunction()
	{
		Verify(CurrentFunction != -1);
		return Functions[CurrentFunction];
	}

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
				FReaderScope Scope(Ptr, WordCount);
				Ptr++;
				SpvExecutionModel ExecModel = (SpvExecutionModel)*Ptr++;
				ProcessExecModel(ExecModel);
				FEntryPoint EntryPoint;
				EntryPoint.Id = *Ptr++;
				EntryPoint.Name = ReadLiteralString(Ptr);
				int32_t NumInterfaces = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(NumInterfaces >= 0);
				for (int32_t Index = 0; Index < NumInterfaces; ++Index)
				{
					EntryPoint.Interfaces.push_back(*Ptr++);
				}
				EntryPoints.push_back(EntryPoint);
				bAutoIncrease = false;
			}
			break;
			case SpvOpExecutionMode:
			case SpvOpSource:
			case SpvOpSourceExtension:
				break;
			case SpvOpName:
			{
				FReaderScope Scope(Ptr, WordCount);
				Ptr++;
				uint32_t Target = *Ptr++;
				auto Name = ReadLiteralString(Ptr);
				Names[Target] = Name;
				bAutoIncrease = false;
			}
				break;
			case SpvOpMemberName:
			{
				FReaderScope Scope(Ptr, WordCount);
				Ptr++;
				uint32_t StructType = *Ptr++;
				FMemberName MemberName;
				MemberName.MemberIndex = *Ptr++;
				MemberName.Name = ReadLiteralString(Ptr);
				MemberNames[StructType].push_back(MemberName);
				bAutoIncrease = false;
			}
				break;
			case SpvOpDecorate:
			{
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t Target = *Ptr++;
				FDecoration Decoration;
				Decoration.Decoration = (SpvDecoration)*Ptr++;
				Decoration.Literals = ReadDecoration(Ptr, Decoration.Decoration);				

				Decorations[Target].push_back(Decoration);
				bAutoIncrease = false;
			}
				break;
			case SpvOpMemberDecorate:
			{
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t StructureType = *Ptr++;
				FMemberDecoration Decoration;
				Decoration.MemberIndex = *Ptr++;
				Decoration.Decoration = (SpvDecoration)*Ptr++;
				Decoration.Literals = ReadDecoration(Ptr, Decoration.Decoration);
				MemberDecorations[StructureType].push_back(Decoration);
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeVoid:
			{
				FType Type;
				Type.Type = FType::EType::Void;
				++Ptr;
				uint32_t Result = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeFunction:
			{
				FType Type;
				Type.Type = FType::EType::Function;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.Function.ReturnType = *Ptr++;
				int32_t NumParams = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumParams; ++Index)
				{
					uint32_t ParamType = *Ptr++;
					Type.Function.ParameterTypes.push_back(ParamType);
				}
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeInt:
			{
				FType Type;
				Type.Type = FType::EType::Int;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.Width = *Ptr++;
				Type.Signedness = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeFloat:
			{
				FType Type;
				Type.Type = FType::EType::Int;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.Width = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeBool:
			{
				FType Type;
				Type.Type = FType::EType::Bool;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypePointer:
			{
				FType Type;
				Type.Type = FType::EType::Pointer;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.Pointer.StorageClass = (SpvStorageClass)*Ptr++;
				Type.Pointer.Type = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeVector:
			{
				FType Type;
				Type.Type = FType::EType::Vector;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.Vector.ComponentType = *Ptr++;
				Type.Vector.NumComponents = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeArray:
			{
				FType Type;
				Type.Type = FType::EType::Array;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.Array.ElementType = *Ptr++;
				Type.Array.Length = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpConstant:
			{
				FConstant Constant;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Constant.Type = *Ptr++;
				uint32_t Result = *Ptr++;
				int32_t NumValues = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumValues; ++Index)
				{
					uint32_t Value = *Ptr++;
					Constant.Values.push_back(Value);
				}
				Constants[Result] = Constant;
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeStruct:
			{
				FType Type;
				Type.Type = FType::EType::Struct;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				int32_t NumMembers = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumMembers; ++Index)
				{
					uint32_t MemberType = *Ptr++;
					Type.Struct.MemberTypes.push_back(MemberType);
				}
				Types[ResultType] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpVariable:
			{
				FVariable Variable;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Variable.Type = *Ptr++;
				uint32_t Result = *Ptr++;
				Variable.StorageClass = (SpvStorageClass)*Ptr++;
				int32_t HasInitializer = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasInitializer == 0 || HasInitializer == 1);
				Variable.bInitializer = true;
				if (HasInitializer == 1)
				{
					uint32_t Initializer = *Ptr++;
					Variable.Initializer = Initializer;
				}
				Variables[Result] = Variable;
				GetInstructions().push_back(Result);
				bAutoIncrease = false;
			}
				break;
			case SpvOpTypeImage:
			{
				FType Type;
				Type.Type = FType::EType::Image;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				Type.Image.SampledType = *Ptr++;
				Type.Image.Dim = (SpvDim)*Ptr++;
				Type.Image.Depth = *Ptr++;
				Type.Image.Arrayed = *Ptr++;
				Type.Image.MS = *Ptr++;
				Type.Image.Sampled = *Ptr++;
				Type.Image.ImageFormat = (SpvImageFormat)*Ptr++;

				int32_t HasQualifier = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasQualifier == 0 || HasQualifier == 1);
				if (HasQualifier == 1)
				{
					Type.Image.Qualifier = (SpvAccessQualifier)*Ptr++;
				}
				Types[ResultType] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpConstantComposite:
			{
				FConstantComposite Constant;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Constant.Type = *Ptr++;
				uint32_t Result = *Ptr++;
				int32_t NumElements = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumElements; ++Index)
				{
					uint32_t Element = *Ptr++;
					Constant.Constituents.push_back(Element);
				}
				ConstantComposites[Result] = Constant;
				bAutoIncrease = false;
			}
			break;
			case SpvOpCompositeConstruct:
			{
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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
				FType Type;
				Type.Type = FType::EType::SampledImage;
				*Ptr++;
				uint32_t Result = *Ptr++;
				Type.ImageType = *Ptr++;
				Types[Result] = Type;
				bAutoIncrease = false;
			}
				break;
			case SpvOpFunction:
			{
				FFunction Function;
				*Ptr++;
				Function.ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				Function.FunctionControl = /*SpvFunctionControlMask*/*Ptr++;
				Function.FunctionType = *Ptr++;
				Functions[Result] = Function;
				Verify(CurrentFunction == 0);
				CurrentFunction = Result;
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
				Verify(false);
				*Ptr++;
				uint32_t Result = *Ptr++;
				bAutoIncrease = false;
			}
				break;
			case SpvOpLabel:
			{
				auto& Function = GetFunction();
				*Ptr++;
				uint32_t Result = *Ptr++;
				FBlock Block;
				Block.Label = Result;
				Function.Blocks.push_back(Block);
				bAutoIncrease = false;
			}
				break;
			case SpvOpBranch:
			{
				Verify(false);
				*Ptr++;
				uint32_t Result = *Ptr++;
				bAutoIncrease = false;
			}
				break;
			case SpvOpAccessChain:
			{
				FAccessChain AccessChain;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				AccessChain.Type = *Ptr++;
				uint32_t Result = *Ptr++;
				AccessChain.Base = *Ptr++;
				int32_t NumIndices = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumIndices; ++Index)
				{
					uint32_t ChainIndex = *Ptr++;
					AccessChain.Indices.push_back(ChainIndex);
				}
				AccessChains[Result] = AccessChain;
				GetInstructions().push_back(Result);
				bAutoIncrease = false;
			}
				break;
			case SpvOpLoad:
			{
				FLoad Load;
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Load.ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				Load.Pointer = *Ptr++;
				int32_t HasMemoryAccess = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasMemoryAccess == 0 || HasMemoryAccess == 1);
				Load.bHasMemoryAccess = HasMemoryAccess != 0;
				if (HasMemoryAccess == 1)
				{
					uint32_t MemoryAccess = /*SpvMemoryAccessMask*/*Ptr++;
					Load.MemoryAccess = MemoryAccess;
				}
				Loads[Result] = Load;
				GetInstructions().push_back(Result);
				bAutoIncrease = false;
			}
				break;
			case SpvOpStore:
			{
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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
				Verify(false);
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
				Verify(false);
				*Ptr++;
				uint32_t MergeBlock = *Ptr++;
				uint32_t SelectionControl = /*SpvSelectionControlMask*/*Ptr++;
				bAutoIncrease = false;
			}
				break;
			case SpvOpConvertFToS:
			{
				Verify(false);
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
				Verify(false);
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
				Verify(false);
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
				Verify(false);
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
				Verify(false);
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
				Verify(false);
				*Ptr++;
				uint32_t ResultType = *Ptr++;
				uint32_t Result = *Ptr++;
				uint32_t SampledImage = *Ptr++;
				bAutoIncrease = false;
			}
				break;
			case SpvOpImageSampleImplicitLod:
			{
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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
				Verify(false);
				FReaderScope Scope(Ptr, WordCount);
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

