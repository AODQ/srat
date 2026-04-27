#pragma once

// batch render all glTF-Sample-Assets models to PNG files.
// requires a raylib window to already be initialized (for
// texture loading via LoadImage and PNG export via ExportImage).

void batch_render_all_models(
	char const * modelsBasePath,
	char const * outputDir
);
