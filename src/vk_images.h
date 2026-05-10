
#pragma once 

namespace vkutil {

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	// Aspect-explicit overload — needed for depth images transitioning into a
	// non-depth layout like SHADER_READ_ONLY_OPTIMAL, where the stock helper's
	// "is the new layout depth?" check picks the wrong aspect.
	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);
	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

};