#include "RowColMapEncoder.h"
#include "MapCommon.h"
#include <vector>
#include <algorithm>

namespace Origins
{

struct FillInfo
{
	int rows[c_tilesPerLine];
	int cols[c_tilesPerLine];
	uint64 canvasRows[c_tilesPerLine][c_tilesCanvasWidthHeight];
	uint64 canvasCols[c_tilesPerLine][c_tilesCanvasWidthHeight];
};

inline static uint32 tileIndex(int x, int y)
{
	return y * c_tilesCanvasWidthHeight + x;
}

inline static uint64 tileValueHorizontal(uint8* canvas, int x, int y, int line)
{
	uint64 result = 0;

	const uint8* p = canvas + (y + line) * c_tilesCanvasWidthHeight + x;
	for (int i = 0; i < c_tileWidthHeight; i++)
	{
		result <<= 4;
		result |= *p++;
	}

	return result;
}

inline static uint64 tileValueVertical(uint8* canvas, int x, int y, int col)
{
	uint64 result = 0;

	const uint8* p = canvas + y * c_tilesCanvasWidthHeight + x + col;
	for (int i = 0; i < c_tileWidthHeight; i++)
	{
		result <<= 4;
		result |= *p;
		p += c_tilesCanvasWidthHeight;
	}

	return result;
}

static void calcPrefix(const uint8* pattern, int patternSize, char* prefix)
{
	prefix[0] = 0;
	int k = 0;
	for(int i = 1; i < patternSize; i++)
	{
		while (k > 0 && pattern[k] != pattern[i])
		{
			k = prefix[k - 1];
		}
		if (pattern[k] == pattern[i])
		{
			k++;
		}
		prefix[i] = k;
	}
}

static void copyRect(const uint8* image, int width, int, int tileX, int tileY, uint8* canvas, int canvasX, int canvasY)
{
	for (int y = 0; y < c_tileWidthHeight; y++)
	{
		uint8* canvasPixels = canvas + (canvasY + y) * c_tilesCanvasWidthHeight + canvasX;
		const uint8* imagePixels = image + (tileY + y) * width + tileX;
		std::copy(imagePixels, imagePixels + c_tileWidthHeight, canvasPixels);
	}
}

static bool compareTile(int compareWidth, int compareHeight, uint8* canvas, int canvasX, int canvasY, const uint8* image, int width, int, int tileX, int tileY)
{
	for (int y = 0; y < compareHeight; y++)
	{
		const uint8* canvasPixels = canvas + (canvasY + y) * c_tilesCanvasWidthHeight + canvasX;
		const uint8* imagePixels = image + (tileY + y) * width + tileX;
		if (!std::equal(canvasPixels, canvasPixels + compareWidth, imagePixels))
		{
			return false;
		}
	}

	return true;
}


struct TilePlaceInfo
{
	TilePlaceInfo(int x = -1, int y = -1, int weight = -1)
		: x(x)
		, y(y)
		, weight(weight)
	{
	}

	bool isNull() const
	{
		return (weight == -1);
	}

	int x;
	int y;
	int weight;
};

static TilePlaceInfo findBestTilePlace(const FillInfo& fillInfo, uint8* canvas, uint8* image, int width, int height, int tileX, int tileY)
{
	TilePlaceInfo result;

	for (int rcOffset = 0; rcOffset < c_tilesCanvasWidthHeight; rcOffset += c_tileWidthHeight)
	{
		const int rc = rcOffset / c_tileWidthHeight;

		const int minLookupX = std::max(rcOffset, fillInfo.rows[rc] - c_tileWidthHeight);
		const int maxLookupX = std::max(rcOffset, std::min(c_tilesCanvasWidthHeight - c_tileWidthHeight, fillInfo.rows[rc]));
		for (int x = minLookupX; x <= maxLookupX; x++)
		{
			const int weight = std::min<int>(c_tileWidthHeight, std::max<int>(0, fillInfo.rows[rc] - x));
			if (compareTile(weight, c_tileWidthHeight, canvas, x, rcOffset, image, width, height, tileX, tileY))
			{
				if (weight > result.weight)
				{
					result.weight = weight;
					result.x = x;
					result.y = rcOffset;
					if (result.weight == c_tileWidthHeight)
					{
						return result;
					}
				}
			}
		}

		const int minLookupY = std::max(rcOffset, fillInfo.cols[rc] - c_tileWidthHeight);
		const int maxLookupY = std::max(rcOffset, std::min(c_tilesCanvasWidthHeight - c_tileWidthHeight, fillInfo.cols[rc]));
		for (int y = minLookupY; y <= maxLookupY; y++)
		{
			const int weight = std::min<int>(c_tileWidthHeight, std::max<int>(0, fillInfo.cols[rc] - y));
			if (compareTile(c_tileWidthHeight, weight, canvas, rcOffset, y, image, width, height, tileX, tileY))
			{
				if (weight > result.weight)
				{
					result.weight = weight;
					result.x = rcOffset;
					result.y = y;
					if (result.weight == c_tileWidthHeight)
					{
						return result;
					}
				}
			}
		}
	}

	return result;
}

struct TileInfo
{
	uint32* index;
	int x;
	int y;
};

static bool placeBestSuitableTile(std::vector<TileInfo>& tiles, FillInfo& fillInfo, uint8* canvas, uint8* image, int width, int height)
{
	TilePlaceInfo bestPlace;
	int bestTileIndex = -1;

	for (size_t i = 0; i < tiles.size(); i++)
	{
		const TilePlaceInfo place = findBestTilePlace(fillInfo, canvas, image, width, height, tiles[i].x, tiles[i].y);
		
		if (place.weight > bestPlace.weight)
		{
			bestPlace = place;
			bestTileIndex = i;

			if (bestPlace.weight == c_tileWidthHeight)
			{
				break;
			}
		}
	}

	if (!bestPlace.isNull())
	{
		ASSERT(bestTileIndex >= 0);

		const int row = bestPlace.x / c_tileWidthHeight;
		const int col = bestPlace.y / c_tileWidthHeight;
		fillInfo.rows[col] = std::max<int>(fillInfo.rows[col], bestPlace.x + c_tileWidthHeight);
		fillInfo.cols[row] = std::max<int>(fillInfo.cols[row], bestPlace.y + c_tileWidthHeight);

		copyRect(image, width, height, tiles[bestTileIndex].x, tiles[bestTileIndex].y, canvas, bestPlace.x, bestPlace.y);

		*tiles[bestTileIndex].index = tileIndex(bestPlace.x, bestPlace.y);
		std::swap(tiles[bestTileIndex], tiles.back());
		tiles.pop_back();	

		return true;
	}

	return false;
}

static bool isEmptyTile(uint8* image, int width, int, int tileX, int tileY)
{
	for (int y = 0; y < c_tileWidthHeight; y++)
	{
		const uint8* imagePixels = image + (tileY + y) * width + tileX;
		for (int x = 0; x < c_tileWidthHeight; x++)
		{
			if (*imagePixels++ != 0)
			{
				return false;
			}
		}
	}

	return true;
}

bool RowColMapEncoder::encodeLayer(FillInfo& fillInfo, const void* pixels, int width, int height, uint8* canvas, uint32* palette, uint32* indices)
{
	const int tileCount = (width * height) / (c_tileWidthHeight * c_tileWidthHeight);

	std::fill_n(indices, tileCount, c_nullTileIndex);

	std::vector<uint8> indexedImage(width * height);
	
	if (!quantize(pixels, width, height, &indexedImage[0], palette, true))
	{
		return false;
	}

	std::vector<TileInfo> tiles;
	tiles.reserve(tileCount);

	uint32* tileIndex = indices;
	for (int y = 0; y < height; y += c_tileWidthHeight)
	{
		for (int x = 0; x < width; x += c_tileWidthHeight)
		{
			if (isEmptyTile(&indexedImage[0], width, height, x, y))
			{
				*tileIndex++ = c_nullTileIndex;
				continue;
			}

			TileInfo info;
			info.index = tileIndex++;
			info.x = x;
			info.y = y;
			tiles.push_back(info);
		}
	}

	while (!tiles.empty())
	{
		if (!placeBestSuitableTile(tiles, fillInfo, canvas, &indexedImage[0], width, height))
		{
			return false;
		}
	}


	return true;
}

bool RowColMapEncoder::encodeLayers(void* tilesCanvas, const void* layer1Pixels, const void* layer0Pixels, uint32* layer1Palette, uint32* layer0Palette, uint32* layer1Indices, uint32* layer0Indices)
{
	std::vector<uint8> canvas(c_tilesCanvasWidthHeight * c_tilesCanvasWidthHeight);

	FillInfo fillInfo;
	std::fill_n(fillInfo.cols, 32, 0);
	std::fill_n(fillInfo.rows, 32, 0);

	if (!encodeLayer(fillInfo, layer1Pixels, c_layer1Width, c_layer1Height, &canvas[0], layer1Palette, layer1Indices)
		|| !encodeLayer(fillInfo, layer0Pixels, c_layer0Width, c_layer0Height, &canvas[0], layer0Palette, layer0Indices)
	)
	{
		return false;
	}

	std::vector<uint8> canvas4bpp(c_tilesCanvasWidthHeight * c_tilesCanvasWidthHeight / 2);
	indexed8ToIndexed4(&canvas[0], &canvas4bpp[0], c_tilesCanvasWidthHeight * c_tilesCanvasWidthHeight);		
	swizzle4(&canvas4bpp[0], tilesCanvas, c_tilesCanvasWidthHeight, c_tilesCanvasWidthHeight);

	return true;
}

}
