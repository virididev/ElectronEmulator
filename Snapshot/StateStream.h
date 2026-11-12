#ifndef SNAPSHOT_STATESTREAM_H
#define SNAPSHOT_STATESTREAM_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

/* A row of bytes that the state of a machine is written to or read back from.

   Whether it is the one or the other, a component passes its state through it
   with the same calls in the same order, so it needs only one function for both
   saving and loading and the two cannot get out of step:

	void Chip::TransferState(StateStream &Stream)
	{
		Stream.Value(Counter);
		Stream.Bytes(Registers, sizeof(Registers));
	}

   Numbers are stored low byte first in a fixed number of bytes, whatever the host
   does in its own memory, so a snapshot made on one kind of computer is good on
   any other. */
class StateStream
{
	public:
		/* a stream to save to */
		StateStream() : IsLoading(false), HasFailed(false), Position(0) {}

		/* a stream to load from */
		explicit StateStream(const std::vector<uint8_t> &Contents) : IsLoading(true), HasFailed(false), Position(0), Data(Contents) {}

		bool Loading() const { return IsLoading; }

		/* true once a stream being loaded has turned out not to hold what was
		asked of it; from then on nothing is read any more */
		bool Failed() const { return HasFailed; }
		void Fail() { HasFailed = true; }

		const std::vector<uint8_t> &Contents() const { return Data; }

		void Bytes(void *Target, size_t Length)
		{
			if(!IsLoading)
			{
				const uint8_t *const Source = (const uint8_t *)Target;
				Data.insert(Data.end(), Source, Source + Length);
			}
			else if(HasFailed || Length > Data.size() - Position)
				HasFailed = true;
			else
			{
				memcpy(Target, &Data[Position], Length);
				Position += Length;
			}
		}

		/* for whole numbers of any size */
		template <typename Type> void Value(Type &Target)
		{
			static_assert(std::is_integral<Type>::value, "only whole numbers can be transferred as they are");

			uint8_t Raw[sizeof(Type)];
			if(!IsLoading)
			{
				const uint64_t Number = (uint64_t)Target;
				for(size_t c = 0; c < sizeof(Type); c++) Raw[c] = (uint8_t)(Number >> (8*c));
			}

			Bytes(Raw, sizeof(Type));

			if(IsLoading && !HasFailed)
			{
				uint64_t Number = 0;
				for(size_t c = 0; c < sizeof(Type); c++) Number |= (uint64_t)Raw[c] << (8*c);
				Target = (Type)Number;
			}
		}

		void Value(bool &Target)
		{
			uint8_t Flag = Target ? 1 : 0;
			Value(Flag);
			if(IsLoading && !HasFailed) Target = Flag != 0;
		}

		void Value(float &Target)
		{
			uint32_t Bits;
			memcpy(&Bits, &Target, sizeof(Bits));
			Value(Bits);
			if(IsLoading && !HasFailed) memcpy(&Target, &Bits, sizeof(Bits));
		}

		/* for sizes and positions, which are not the same number of bytes on
		every host */
		void Size(size_t &Target)
		{
			uint64_t Number = Target;
			Value(Number);
			if(IsLoading && !HasFailed) Target = (size_t)Number;
		}

		/* A number that has to be the same on loading as it was on saving, such as
		a version number; if it is not, the stream fails before any harm is done */
		void Expect(int Expected)
		{
			int Found = Expected;
			Value(Found);
			if(Found != Expected) HasFailed = true;
		}

		void Vector(std::vector<uint8_t> &Target)
		{
			uint64_t Length = Target.size();
			Value(Length);

			if(IsLoading)
			{
				if(HasFailed || Length > Data.size() - Position)
				{
					HasFailed = true;
					return;
				}
				Target.resize((size_t)Length);
			}

			if(Length) Bytes(Target.data(), (size_t)Length);
		}

		void String(std::string &Target)
		{
			std::vector<uint8_t> Characters(Target.begin(), Target.end());
			Vector(Characters);
			if(IsLoading && !HasFailed) Target.assign(Characters.begin(), Characters.end());
		}

		/* the same for a piece of text */
		void Expect(const std::string &Expected)
		{
			std::string Found = Expected;
			String(Found);
			if(Found != Expected) HasFailed = true;
		}

	private:
		bool IsLoading, HasFailed;
		size_t Position;
		std::vector<uint8_t> Data;
};

#endif
