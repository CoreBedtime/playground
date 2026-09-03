//Created by Salty on 8/26/26.

#import <Foundation/Foundation.h>
#import <Swingset/Swingset.h>

@interface PPTweaksCollection : NSObject <SOPPObservableDictionaryDelegate>
@property (strong, nonatomic) SOPPObservableDictionary *collectionDictionary;
///Setting this BOOL will reload the entire collection.
- (void)setIsForLegacyAmmonia:(BOOL)isForLegacyAmmonia;
- (void)disableTweakAtPath:(NSString *)tweakPath;
- (void)enableTweakAtPath:(NSString *)tweakPath;
- (void)packageTweakAtPath:(NSString *)tweakPath;
@property (assign) BOOL isForLegacyAmmonia;
+ (instancetype)loadCurrentTweaksContentsUsingAmmonia:(BOOL)ammonia;
@end
