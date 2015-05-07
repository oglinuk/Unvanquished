#ifndef ACIDTUBE_COMPONENT_H_
#define ACIDTUBE_COMPONENT_H_

#include "../backend/CBSEBackend.h"
#include "../backend/CBSEComponents.h"

class AcidTubeComponent: public AcidTubeComponentBase {
	public:
		// ///////////////////// //
		// Autogenerated Members //
		// ///////////////////// //

		/**
		 * @brief Default constructor of the AcidTubeComponent.
		 * @param entity The entity that owns the component instance.
		 * @param r_AlienBuildableComponent A AlienBuildableComponent instance that this component depends on.
		 * @note This method is an interface for autogenerated code, do not modify its signature.
		 */
		AcidTubeComponent(Entity& entity, AlienBuildableComponent& r_AlienBuildableComponent);

		// ///////////////////// //

	private:

};

#endif // ACIDTUBE_COMPONENT_H_
