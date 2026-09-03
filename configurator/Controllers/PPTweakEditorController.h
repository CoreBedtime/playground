//Created by Salty on 9/2/26.

#import <Cocoa/Cocoa.h>

@interface PPTweakEditorController : NSViewController <NSTableViewDelegate, NSTableViewDataSource>
- (instancetype)initWithTitle:(NSString *)tweakName
              optionsDictPath:(NSString *)optionsPath
             tweakOptionsDict:(NSDictionary *)options;

@property (weak) IBOutlet NSTableView *blacklistTableView;
@property (weak) IBOutlet NSTableView *whitelistTableView;
@property (weak) IBOutlet NSTableView *frameworkDependenciesTableView;
@end
