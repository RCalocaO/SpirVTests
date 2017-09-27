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

struct FInstruction
{
	FInstruction(SpvOp InOpCode)
		: OpCode(InOpCode)
	{
	}

	SpvOp OpCode = SpvOpNop;
	uint32_t Result = 0;

	struct FVectorShuffle
	{
		uint32_t ResultType = 0;
		uint32_t Vector1 = 0;
		uint32_t Vector2 = 0;
		std::vector<uint32_t> Components;
	} VectorShuffle;

	struct FImage
	{
		uint32_t ResultType = 0;
		uint32_t SampledImage = 0;
	} Image;

	struct FImageFetch
	{
		uint32_t ResultType = 0;
		uint32_t Image = 0;
		uint32_t Coordinate = 0;
		std::vector<SpvImageOperandsMask> ImageOperands;
	} ImageFetch;

	struct FImageSampleImplicitLod
	{
		uint32_t ResultType = 0;
		uint32_t SampledImage = 0;
		uint32_t Coordinate = 0;
		std::vector<SpvImageOperandsMask> Operands;
	} ImageSampleImplicitLod;

	struct FCompositeExtract
	{
		uint32_t ResultType = 0;
		uint32_t Composite = 0;
		std::vector<int32_t> Literals;
	} CompositeExtract;

	struct  FCompositeConstruct
	{
		uint32_t ResultType = 0;
		std::vector<uint32_t> Elements;
	} CompositeConstruct;
	struct FVariable
	{
		uint32_t Type = 0;
		SpvStorageClass StorageClass = SpvStorageClassMax;
		bool bInitializer = false;
		uint32_t Initializer = 0;
	} Var;

	struct FAccessChain
	{
		uint32_t Type = 0;
		uint32_t Base = 0;
		std::vector<uint32_t> Indices;
	} AccessChain;

	struct FLoad
	{
		uint32_t ResultType = 0;
		uint32_t Pointer = 0;
		bool bHasMemoryAccess = false;
		uint32_t MemoryAccess = 0;
	} Load;

	struct FStore
	{
		uint32_t Pointer = 0;
		uint32_t Object = 0;
		bool bHasMemoryAccess = false;
		SpvMemoryAccessMask MemoryAccess = (SpvMemoryAccessMask)0;
	} Store;

	struct FExpression
	{
		uint32_t ResultType = 0;
		uint32_t Op1 = 0;
		uint32_t Op2 = 0;
	} Expr;

	struct FBranch
	{
		uint32_t Target = 0;
	} Branch;

	struct FBranchConditional
	{
		uint32_t Condition = 0;
		uint32_t True = 0;
		uint32_t False = 0;
	} BranchConditional;
};

struct FConstant
{
	uint32_t Type = 0;
	std::vector<uint32_t> Values;
};

struct FConstantComposite
{
	uint32_t Type = 0;
	std::vector<uint32_t> Constituents;
};

struct FBlock
{
	uint32_t Label = 0;
	std::vector<FInstruction> Instructions;
};

struct FFunction
{
	uint32_t ResultType = 0;
	uint32_t FunctionControl = 0;
	uint32_t FunctionType = 0;

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

	std::map<uint32_t, FFunction> Functions;

	uint32_t CurrentFunction = 0;

	std::vector<FInstruction> Instructions;

	std::vector<FInstruction>& GetInstructions()
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
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				FInstruction Instruction(OpCode);
				Instruction.Var.Type = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.Var.StorageClass = (SpvStorageClass)*Ptr++;
				int32_t HasInitializer = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasInitializer == 0 || HasInitializer == 1);
				Instruction.Var.bInitializer = true;
				if (HasInitializer == 1)
				{
					uint32_t Initializer = *Ptr++;
					Instruction.Var.Initializer = Initializer;
				}
				GetInstructions().push_back(Instruction);
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
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.CompositeConstruct.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				int32_t NumElements = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumElements; ++Index)
				{
					uint32_t Element = *Ptr++;
					Instruction.CompositeConstruct.Elements.push_back(Element);
				}
				GetInstructions().push_back(Instruction);
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
				CurrentFunction = 0;
				break;
			case SpvOpReturn:
				break;
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
				FInstruction Instruction(OpCode);
				*Ptr++;
				Instruction.Branch.Target = *Ptr++;
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpAccessChain:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.AccessChain.Type = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.AccessChain.Base = *Ptr++;
				int32_t NumIndices = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumIndices; ++Index)
				{
					uint32_t ChainIndex = *Ptr++;
					Instruction.AccessChain.Indices.push_back(ChainIndex);
				}
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpLoad:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.Load.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.Load.Pointer = *Ptr++;
				int32_t HasMemoryAccess = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasMemoryAccess == 0 || HasMemoryAccess == 1);
				Instruction.Load.bHasMemoryAccess = HasMemoryAccess != 0;
				if (HasMemoryAccess == 1)
				{
					uint32_t MemoryAccess = /*SpvMemoryAccessMask*/*Ptr++;
					Instruction.Load.MemoryAccess = MemoryAccess;
				}
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpStore:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.Store.Pointer = *Ptr++;
				Instruction.Store.Object = *Ptr++;
				uint32_t HasMemoryAccess = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(HasMemoryAccess == 0 || HasMemoryAccess == 1);
				Instruction.Store.bHasMemoryAccess = HasMemoryAccess != 0;
				if (HasMemoryAccess == 1)
				{
					Instruction.Store.MemoryAccess = (SpvMemoryAccessMask)*Ptr++;
				}
				GetInstructions().push_back(Instruction);
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
			case SpvOpSelectionMerge:
			{
				// Target Block after selection block
				*Ptr++;
				uint32_t MergeBlock = *Ptr++;
				SpvSelectionControlMask SelectionControl = (SpvSelectionControlMask)*Ptr++;
				Verify(SelectionControl == 0);
				bAutoIncrease = false;
			}
				break;
			case SpvOpFNegate:
			case SpvOpConvertFToS:
			case SpvOpAny:
			case SpvOpAll:
			{
				FInstruction Instruction(OpCode);
				*Ptr++;
				Instruction.Expr.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.Expr.Op1 = *Ptr++;
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpINotEqual:
			case SpvOpFAdd:
			case SpvOpFDiv:
			case SpvOpFSub:
			case SpvOpFMul:
			case SpvOpFMod:
			case SpvOpDot:
			case SpvOpOuterProduct:
			case SpvOpFOrdEqual:
			case SpvOpFOrdNotEqual:
			case SpvOpFOrdLessThanEqual:
			case SpvOpFOrdLessThan:
			case SpvOpFOrdGreaterThan:
			case SpvOpFOrdGreaterThanEqual:
			{
				FInstruction Instruction(OpCode);
				*Ptr++;
				Instruction.Expr.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.Expr.Op1 = *Ptr++;
				Instruction.Expr.Op2 = *Ptr++;
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpImage:
			{
				FInstruction Instruction(OpCode);
				*Ptr++;
				Instruction.Image.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.Image.SampledImage = *Ptr++;
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpImageSampleImplicitLod:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.ImageSampleImplicitLod.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.ImageSampleImplicitLod.SampledImage = *Ptr++;
				Instruction.ImageSampleImplicitLod.Coordinate = *Ptr++;
				int32_t NumOperands = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumOperands; ++Index)
				{
					SpvImageOperandsMask Operand = (SpvImageOperandsMask)*Ptr++;
					Instruction.ImageSampleImplicitLod.Operands.push_back(Operand);
				}
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpBranchConditional:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.BranchConditional.Condition = *Ptr++;
				Instruction.BranchConditional.True = *Ptr++;
				Instruction.BranchConditional.False = *Ptr++;
				int32_t NumBranchWeights = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				Verify(NumBranchWeights == 0);
				for (int32_t Index = 0; Index < NumBranchWeights; ++Index)
				{
					uint32_t BranchWeight = *Ptr++;
				}
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpVectorShuffle:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.VectorShuffle.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.VectorShuffle.Vector1 = *Ptr++;
				Instruction.VectorShuffle.Vector2 = *Ptr++;
				int32_t NumComponents = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumComponents; ++Index)
				{
					uint32_t Component = *Ptr++;
					Instruction.VectorShuffle.Components.push_back(Component);
				}
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpImageFetch:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.ImageFetch.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.ImageFetch.Image = *Ptr++;
				Instruction.ImageFetch.Coordinate = *Ptr++;
				int32_t NumComponents = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumComponents; ++Index)
				{
					SpvImageOperandsMask ImageOperands = (SpvImageOperandsMask)*Ptr++;
					Instruction.ImageFetch.ImageOperands.push_back(ImageOperands);
				}
				GetInstructions().push_back(Instruction);
				bAutoIncrease = false;
			}
				break;
			case SpvOpCompositeExtract:
			{
				FInstruction Instruction(OpCode);
				FReaderScope Scope(Ptr, WordCount);
				*Ptr++;
				Instruction.CompositeExtract.ResultType = *Ptr++;
				Instruction.Result = *Ptr++;
				Instruction.CompositeExtract.Composite = *Ptr++;
				int32_t NumComponents = (int32_t)((Scope.SrcPtr + WordCount) - Ptr);
				for (int32_t Index = 0; Index < NumComponents; ++Index)
				{
					int32_t Literal = *Ptr++;
					Instruction.CompositeExtract.Literals.push_back(Literal);
				}
				GetInstructions().push_back(Instruction);
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

int main(int argc, char** argv)
{
	if (argc > 1)
	{
		for (int i = 1; i < argc; ++i)
		{
			FSpirVParser Parser;
			if (!Parser.Init(argv[i]))
			{
				return 1;
			}

			Parser.Process();
		}
	}
	else
	{
		FSpirVParser Parser;
		if (!Parser.Init(SPVFile))
		{
			return 1;
		}

		Parser.Process();
	}

	return 0;
}
