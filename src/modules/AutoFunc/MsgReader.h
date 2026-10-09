#pragma once

#include <cstdint>
#include <cstring>
#include <string>

// Minimal user-message reader for this plugin's event signals (plan 04 §3:
// signals are parsed in-plugin, not through AbcUi). Same framing semantics as
// ABCEnchance src/core/library/NetworkMessageReader.*: little-endian,
// readCoord = int32 * (1/8); a truncated read flags readOK() == false and the
// hook must fall back to the original handler. The full reader lands with the
// AbcUi data layer (plan 05 §4).
class CMsgReader {
public:
	CMsgReader(const void* pBuf, int iSize)
		: m_pBuffer(static_cast<const uint8_t*>(pBuf)), m_iSize(iSize) {
		if (!m_pBuffer && m_iSize > 0)
			m_bError = true;
	}

	bool readOK() const { return !m_bError; }

	int8_t readChar() {
		if (m_iPos + 1 > m_iSize) {
			m_bError = true;
			return 0;
		}
		return static_cast<int8_t>(m_pBuffer[m_iPos++]);
	}

	uint8_t readByte() {
		if (m_iPos + 1 > m_iSize) {
			m_bError = true;
			return 0;
		}
		return m_pBuffer[m_iPos++];
	}

	int16_t readShort() {
		if (m_iPos + 2 > m_iSize) {
			m_bError = true;
			return 0;
		}
		int16_t val = static_cast<int16_t>(m_pBuffer[m_iPos] | (m_pBuffer[m_iPos + 1] << 8));
		m_iPos += 2;
		return val;
	}

	int32_t readLong() {
		if (m_iPos + 4 > m_iSize) {
			m_bError = true;
			return 0;
		}
		int32_t val = m_pBuffer[m_iPos] | (m_pBuffer[m_iPos + 1] << 8) |
			(m_pBuffer[m_iPos + 2] << 16) | (static_cast<int32_t>(m_pBuffer[m_iPos + 3]) << 24);
		m_iPos += 4;
		return val;
	}

	float readFloat() {
		if (m_iPos + 4 > m_iSize) {
			m_bError = true;
			return 0;
		}
		uint32_t bytes = static_cast<uint32_t>(m_pBuffer[m_iPos] | (m_pBuffer[m_iPos + 1] << 8) |
			(m_pBuffer[m_iPos + 2] << 16) | (m_pBuffer[m_iPos + 3] << 24));
		m_iPos += 4;
		float out;
		static_assert(sizeof(out) == sizeof(bytes), "float size mismatch");
		memcpy(&out, &bytes, sizeof(out));
		return out;
	}

	std::string readString() {
		std::string result;
		while (m_iPos < m_iSize) {
			char c = static_cast<char>(readByte());
			if (c == '\0')
				break;
			result += c;
		}
		return result;
	}

	float readCoord() {
		auto val = readLong();
		if (!readOK())
			return 0;
		return static_cast<float>(val) * (1.0f / 8.0f);
	}

private:
	const uint8_t* m_pBuffer = nullptr;
	int m_iSize = 0;
	int m_iPos = 0;
	bool m_bError = false;
};
