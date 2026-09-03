#import <Foundation/Foundation.h>

@class SOPPObservableDictionary;

@protocol SOPPObservableDictionaryDelegate <NSObject>
- (void)dictionary:(SOPPObservableDictionary *)dict objectWithKey:(id)aKey didGetSetTo:(id)anObject;
- (void)dictionary:(SOPPObservableDictionary *)dict willRemoveObject:(id)anObject forKey:(id)aKey;

@optional
- (void)dictionary:(SOPPObservableDictionary *)dict didRemoveObjectWithKey:(id)aKey;
- (void)dictionary:(SOPPObservableDictionary *)dict objectWithKey:(id)aKey willGetSetTo:(id)anObject;
@end

@interface SOPPObservableDictionary : NSObject

@property (nonatomic, weak) id<SOPPObservableDictionaryDelegate> delegate;
@property (strong) NSString *identifier;

- (instancetype)initWithDelegate:(id<SOPPObservableDictionaryDelegate>)delegate;
- (void)setObject:(id)anObject forKey:(id<NSCopying>)aKey;
- (void)removeObjectForKey:(id)aKey;
- (id)objectForKey:(id)aKey;
- (void)removeAllObjects;
- (NSUInteger)count;
- (NSArray *)allKeys;
- (void)setExistingKey:(id)aKey toNewKey:(id)aNewKey;
@end
