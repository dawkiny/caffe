/**
 * @brief A layer factory that allows one to register layers.
 * During runtime, registered layers could be called by passing a LayerParameter
 * protobuffer to the CreateLayer function:
 *
 *     LayerRegistry<Dtype>::CreateLayer(param);
 *
 * There are two ways to register a layer. Assuming that we have a layer like:
 *
 *   template <typename Dtype, typename Mtype>
 *   class MyAwesomeLayer : public Layer<Dtype,Mtype> {
 *     // your implementations
 *   };
 *
 * and its type is its C++ class name, but without the "Layer" at the end
 * ("MyAwesomeLayer" -> "MyAwesome").
 *
 * If the layer is going to be created simply by its constructor, in your c++
 * file, add the following line:
 *
 *    REGISTER_LAYER_CLASS(MyAwesome);
 *
 * Or, if the layer is going to be created by another creator function, in the
 * format of:
 *
 *    template <typename Dtype, typename Mtype>
 *    Layer<Dtype,Mtype*> GetMyAwesomeLayer(const LayerParameter& param) {
 *      // your implementation
 *    }
 *
 * (for example, when your layer has multiple backends, see GetConvolutionLayer
 * for a use case), then you can register the creator function instead, like
 *
 * REGISTER_LAYER_CREATOR(MyAwesome, GetMyAwesomeLayer)
 *
 * Note that each layer type should only be registered once.
 */

#ifndef CAFFE_LAYER_FACTORY_H_
#define CAFFE_LAYER_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "caffe/common.hpp"
#include "caffe/proto/caffe.pb.h"

namespace caffe {

template <typename Dtype, typename Mtype>
class Layer;

template <typename Dtype, typename Mtype>
class LayerRegistry {
 public:
  typedef shared_ptr<Layer<Dtype,Mtype> > (*Creator)(const LayerParameter&);
  typedef std::map<string, Creator> CreatorRegistry;

  static CreatorRegistry& Registry() {
    static CreatorRegistry* g_registry_ = new CreatorRegistry();
    return *g_registry_;
  }

  // Adds a creator.
  static void AddCreator(const string& type, Creator creator) {
    CreatorRegistry& registry = Registry();
    CHECK_EQ(registry.count(type), 0)
        << "Layer type " << type << " already registered.";
    registry[type] = creator;
  }

  // Get a layer using a LayerParameter.
  static shared_ptr<Layer<Dtype,Mtype> > CreateLayer(const LayerParameter& param) {
    if (Caffe::root_solver()) {
      LOG(INFO) << "Creating layer " << param.name();
    }
    const string& type = param.type();
    CreatorRegistry& registry = Registry();
    CHECK_EQ(registry.count(type), 1) << "Unknown layer type: " << type
        << " (known types: " << LayerTypeListString() << ")";
    return registry[type](param);
  }

  static vector<string> LayerTypeList() {
    CreatorRegistry& registry = Registry();
    vector<string> layer_types;
    for (typename CreatorRegistry::iterator iter = registry.begin();
         iter != registry.end(); ++iter) {
      layer_types.push_back(iter->first);
    }
    return layer_types;
  }

 private:
  // Layer registry should never be instantiated - everything is done with its
  // static variables.
  LayerRegistry() {}

  static string LayerTypeListString() {
    vector<string> layer_types = LayerTypeList();
    string layer_types_str;
    for (vector<string>::iterator iter = layer_types.begin();
         iter != layer_types.end(); ++iter) {
      if (iter != layer_types.begin()) {
        layer_types_str += ", ";
      }
      layer_types_str += *iter;
    }
    return layer_types_str;
  }
};


template <typename Dtype, typename Mtype>
class LayerRegisterer {
 public:
  LayerRegisterer(const string& type,
                  shared_ptr<Layer<Dtype,Mtype> > (*creator)(const LayerParameter&)) {
    // LOG(INFO) << "Registering layer type: " << type;
    LayerRegistry<Dtype,Mtype>::AddCreator(type, creator);
  }
};

#define REGISTER_LAYER_CREATOR_CPU(type, creator) \
  static LayerRegisterer<float,float> g_creator_f_##type(#type, creator<float,float>); \
  static LayerRegisterer<double,double> g_creator_d_##type(#type, creator<double,double>)


  // boris: this should actually be selected at runtime.
#if NATIVE_FP16_SUPPORTED
# define REGISTER_LAYER_CREATOR_GPU(type, creator) \
  REGISTER_LAYER_CREATOR_CPU(type, creator); \
  static LayerRegisterer<float16,float16> g_creator_hh_##type(#type, creator<float16, float16>)

# else
# define REGISTER_LAYER_CREATOR_GPU(type, creator) \
     REGISTER_LAYER_CREATOR_CPU(type, creator); \
     static LayerRegisterer<float16,float> g_creator_hf_##type(#type, creator<float16,float>)
#endif


#ifdef CPU_ONLY
#  define REGISTER_LAYER_CREATOR(type, creator) REGISTER_LAYER_CREATOR_CPU(type, creator)
#else
#  define REGISTER_LAYER_CREATOR(type, creator) REGISTER_LAYER_CREATOR_GPU(type, creator)
#endif

#define REGISTER_LAYER_CLASS(type)                                             \
  template <typename Dtype, typename Mtype>                                          \
  shared_ptr<Layer<Dtype,Mtype> > Creator_##type##Layer(const LayerParameter& param) \
  {                                                                                  \
    return shared_ptr<Layer<Dtype,Mtype> >(new type##Layer<Dtype,Mtype>(param));  \
  }                                                                            \
  REGISTER_LAYER_CREATOR(type, Creator_##type##Layer)

}  // namespace caffe

#endif  // CAFFE_LAYER_FACTORY_H_
